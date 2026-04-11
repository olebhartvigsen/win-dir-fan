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

    WIN32_FIND_DATAW fd = {};
    std::wstring pattern = folderPath;
    if (!pattern.empty() && pattern.back() != L'\\')
        pattern += L'\\';
    pattern += L'*';

    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return items;

    do {
        if (fd.cFileName[0] == L'.') continue; // skip . and ..
        // Skip hidden and system files
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;

        FileItem item;
        item.name = fd.cFileName;
        item.fullPath = folderPath;
        if (!item.fullPath.empty() && item.fullPath.back() != L'\\')
            item.fullPath += L'\\';
        item.fullPath += fd.cFileName;
        item.isDirectory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        item.lastWriteTime = fd.ftLastWriteTime;
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

    if (FAILED(hr) || !pFactory)
        return nullptr;

    HBITMAP hBmp = nullptr;
    SIZE sz = { size, size };
    hr = pFactory->GetImage(sz, SIIGBF_RESIZETOFIT, &hBmp);
    pFactory->Release();

    if (FAILED(hr))
        return nullptr;

    return hBmp;
}

HICON FileService::GetShellIcon(const std::wstring& path) {
    // Try SHIL_JUMBO first (256px), then SHIL_EXTRALARGE (48px), then SHIL_LARGE (32px)
    SHFILEINFOW sfi = {};
    DWORD_PTR res = SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi),
                                   SHGFI_SYSICONINDEX | SHGFI_ICON | SHGFI_LARGEICON);
    if (!res) return nullptr;

    // Try jumbo
    {
        IImageList* pList = nullptr;
        if (SUCCEEDED(SHGetImageList(SHIL_JUMBO, IID_IImageList_, (void**)&pList)) && pList) {
            HICON hIcon = nullptr;
            pList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
            pList->Release();
            if (hIcon) return hIcon;
        }
    }

    // Try extralarge
    {
        IImageList* pList = nullptr;
        if (SUCCEEDED(SHGetImageList(SHIL_EXTRALARGE, IID_IImageList_, (void**)&pList)) && pList) {
            HICON hIcon = nullptr;
            pList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
            pList->Release();
            if (hIcon) return hIcon;
        }
    }

    // Try large
    {
        IImageList* pList = nullptr;
        if (SUCCEEDED(SHGetImageList(SHIL_LARGE, IID_IImageList_, (void**)&pList)) && pList) {
            HICON hIcon = nullptr;
            pList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
            pList->Release();
            if (hIcon) return hIcon;
        }
    }

    // Fallback: icon from sfi
    if (sfi.hIcon) return sfi.hIcon;

    return nullptr;
}
