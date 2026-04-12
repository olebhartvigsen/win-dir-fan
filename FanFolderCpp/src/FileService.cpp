#include "pch.h"
#include "FileService.h"
#include <regex>

static const GUID IID_IShellItemImageFactory_ = {
    0xBCC18B79, 0xBA16, 0x442F,
    {0x80, 0xC4, 0x8A, 0x59, 0xC3, 0x0C, 0x46, 0x3B}
};

static const GUID IID_IImageList_ = {
    0x46EB5926, 0x582E, 0x4017,
    {0x9F, 0xDF, 0xE8, 0x99, 0x8D, 0xAA, 0x09, 0x50}
};

std::vector<FileItem> FileService::ScanFolder(const std::wstring& folderPath, int maxItems, bool includeDirs, const std::wstring& filterRegex, ConfigData::SortMode sortMode) {
    std::vector<FileItem> items;
    if (folderPath.empty()) return items;

    // Pre-build path prefix once; reserve to avoid reallocations
    std::wstring prefix = folderPath;
    if (!prefix.empty() && prefix.back() != L'\\')
        prefix += L'\\';

    items.reserve(64);

    WIN32_FIND_DATAW fd = {};
    std::wstring pattern = prefix + L'*';

    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return items;

    do {
        if (fd.cFileName[0] == L'.') continue; // skip . and ..
        // Skip hidden and system files
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;

        FileItem item;
        item.name = fd.cFileName;
        item.fullPath = prefix;
        item.fullPath += fd.cFileName;
        item.isDirectory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        item.lastWriteTime = fd.ftLastWriteTime;
        item.creationTime  = fd.ftCreationTime;
        items.push_back(std::move(item));
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    // Filter: exclude directories if not wanted
    if (!includeDirs) {
        items.erase(std::remove_if(items.begin(), items.end(),
            [](const FileItem& f) { return f.isDirectory; }), items.end());
    }

    // Filter: regex
    if (!filterRegex.empty()) {
        try {
            std::wregex re(filterRegex, std::regex_constants::icase);
            items.erase(std::remove_if(items.begin(), items.end(),
                [&re](const FileItem& f) {
                    return !std::regex_search(f.name, re);
                }), items.end());
        } catch (...) {
            // invalid regex - ignore filter
        }
    }

    switch (sortMode) {
    case ConfigData::SortMode::DateModifiedDesc:
        std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
            if (a.lastWriteTime.dwHighDateTime != b.lastWriteTime.dwHighDateTime)
                return a.lastWriteTime.dwHighDateTime > b.lastWriteTime.dwHighDateTime;
            return a.lastWriteTime.dwLowDateTime > b.lastWriteTime.dwLowDateTime;
        });
        break;
    case ConfigData::SortMode::DateModifiedAsc:
        std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
            if (a.lastWriteTime.dwHighDateTime != b.lastWriteTime.dwHighDateTime)
                return a.lastWriteTime.dwHighDateTime < b.lastWriteTime.dwHighDateTime;
            return a.lastWriteTime.dwLowDateTime < b.lastWriteTime.dwLowDateTime;
        });
        break;
    case ConfigData::SortMode::NameAsc:
        std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
            return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
        });
        break;
    case ConfigData::SortMode::NameDesc:
        std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
            return _wcsicmp(a.name.c_str(), b.name.c_str()) > 0;
        });
        break;
    case ConfigData::SortMode::DateCreatedDesc:
        std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
            if (a.creationTime.dwHighDateTime != b.creationTime.dwHighDateTime)
                return a.creationTime.dwHighDateTime > b.creationTime.dwHighDateTime;
            return a.creationTime.dwLowDateTime > b.creationTime.dwLowDateTime;
        });
        break;
    case ConfigData::SortMode::DateCreatedAsc:
        std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
            if (a.creationTime.dwHighDateTime != b.creationTime.dwHighDateTime)
                return a.creationTime.dwHighDateTime < b.creationTime.dwHighDateTime;
            return a.creationTime.dwLowDateTime < b.creationTime.dwLowDateTime;
        });
        break;
    }

    if ((int)items.size() > maxItems)
        items.resize(maxItems);

    return items;
}

HBITMAP FileService::GetShellBitmap(const std::wstring& path, int size) {
    IShellItem* pItem = nullptr;
    if (FAILED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&pItem))))
        return nullptr;

    IShellItemImageFactory* pFactory = nullptr;
    HRESULT hr = pItem->QueryInterface(IID_IShellItemImageFactory_, (void**)&pFactory);
    pItem->Release();

    if (FAILED(hr) || !pFactory) return nullptr;

    HBITMAP hBmp = nullptr;
    SIZE sz = { size, size };
    // SIIGBF_ICONONLY: registered file-type icon, never a document/thumbnail preview
    hr = pFactory->GetImage(sz, SIIGBF_ICONONLY, &hBmp);
    pFactory->Release();
    return SUCCEEDED(hr) ? hBmp : nullptr;
}

// Uses SIIGBF_RESIZETOFIT — shows actual image content via the shell thumbnail
// handler. Used for WebP and SVG where GDI+ has no native codec.
HBITMAP FileService::GetShellThumbnail(const std::wstring& path, int size) {
    IShellItem* pItem = nullptr;
    if (FAILED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&pItem))))
        return nullptr;

    IShellItemImageFactory* pFactory = nullptr;
    HRESULT hr = pItem->QueryInterface(IID_IShellItemImageFactory_, (void**)&pFactory);
    pItem->Release();

    if (FAILED(hr) || !pFactory) return nullptr;

    HBITMAP hBmp = nullptr;
    SIZE sz = { size, size };
    hr = pFactory->GetImage(sz, SIIGBF_RESIZETOFIT, &hBmp);
    pFactory->Release();
    return SUCCEEDED(hr) ? hBmp : nullptr;
}

// Load an image file directly with GDI+ and return a square HBITMAP thumbnail
// with the original aspect ratio preserved (letterboxed on transparent bg).
HBITMAP FileService::GetImageThumbnail(const std::wstring& path, int size) {
    Gdiplus::Bitmap* src = Gdiplus::Bitmap::FromFile(path.c_str());
    if (!src || src->GetLastStatus() != Gdiplus::Ok) {
        delete src;
        return nullptr;
    }

    int srcW = (int)src->GetWidth();
    int srcH = (int)src->GetHeight();
    if (srcW <= 0 || srcH <= 0) { delete src; return nullptr; }

    float scale = std::min((float)size / srcW, (float)size / srcH);
    int dstW = (int)(srcW * scale);
    int dstH = (int)(srcH * scale);
    int offX = (size - dstW) / 2;
    int offY = (size - dstH) / 2;

    Gdiplus::Bitmap dst(size, size, PixelFormat32bppPARGB);
    {
        Gdiplus::Graphics g(&dst);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.DrawImage(src, offX, offY, dstW, dstH);
    }
    delete src;

    HBITMAP hBmp = nullptr;
    dst.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hBmp);
    return hBmp;
}

bool FileService::IsGdiImageExtension(const std::wstring& path) {
    const wchar_t* dot = wcsrchr(path.c_str(), L'.');
    if (!dot) return false;
    wchar_t ext[16] = {};
    size_t i = 0;
    for (const wchar_t* p = dot; *p && i < 15; ++p, ++i)
        ext[i] = (wchar_t)towlower(*p);
    static const wchar_t* const kExts[] = {
        L".jpg", L".jpeg", L".png", L".gif",
        L".bmp", L".tiff", L".tif", L".svg"
    };
    for (auto e : kExts) if (wcscmp(ext, e) == 0) return true;
    return false;
}

bool FileService::IsShellThumbnailExtension(const std::wstring& path) {
    const wchar_t* dot = wcsrchr(path.c_str(), L'.');
    if (!dot) return false;
    wchar_t ext[16] = {};
    size_t i = 0;
    for (const wchar_t* p = dot; *p && i < 15; ++p, ++i)
        ext[i] = (wchar_t)towlower(*p);
    static const wchar_t* const kExts[] = { L".webp" };
    for (auto e : kExts) if (wcscmp(ext, e) == 0) return true;
    return false;
}

HICON FileService::GetShellIcon(const std::wstring& path) {
    SHFILEINFOW sfi = {};
    DWORD_PTR res = SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi),
                                   SHGFI_SYSICONINDEX | SHGFI_ICON | SHGFI_LARGEICON);
    if (!res) return nullptr;

    static const int kSizes[] = { SHIL_JUMBO, SHIL_EXTRALARGE, SHIL_LARGE };
    for (int sz : kSizes) {
        IImageList* pList = nullptr;
        if (SUCCEEDED(SHGetImageList(sz, IID_IImageList_, (void**)&pList)) && pList) {
            HICON hIcon = nullptr;
            pList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
            pList->Release();
            if (hIcon) return hIcon;
        }
    }
    return sfi.hIcon;
}
