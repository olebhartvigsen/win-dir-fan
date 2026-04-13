#include "pch.h"
#include "FileService.h"
#include <lunasvg.h>
#include <regex>
#include <shlobj.h>
#include <ole2.h>
#include <unordered_map>

static const GUID IID_IShellItemImageFactory_ = {
    0xBCC18B79, 0xBA16, 0x442F,
    {0x80, 0xC4, 0x8A, 0x59, 0xC3, 0x0C, 0x46, 0x3B}
};

static const GUID IID_IImageList_ = {
    0x46EB5926, 0x582E, 0x4017,
    {0x9F, 0xDF, 0xE8, 0x99, 0x8D, 0xAA, 0x09, 0x50}
};

// Resolves a .lnk shortcut file to its target path.
// Returns the target path, or empty if resolution fails or target doesn't exist.
static std::wstring ResolveLnk(const std::wstring& lnkPath, bool& outIsDir) {
    outIsDir = false;
    IShellLinkW* psl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, (void**)&psl)))
        return {};
    std::wstring result;
    IPersistFile* ppf = nullptr;
    if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
        if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ))) {
            wchar_t target[MAX_PATH] = {};
            if (SUCCEEDED(psl->GetPath(target, MAX_PATH, nullptr, SLGP_RAWPATH)) && target[0]) {
                DWORD attr = GetFileAttributesW(target);
                if (attr != INVALID_FILE_ATTRIBUTES) {
                    result = target;
                    outIsDir = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
                }
            }
        }
        ppf->Release();
    }
    psl->Release();
    return result;
}

// Returns true for executable/application file extensions that should be
// excluded when showing "recent documents" (Jump List task entries link to apps).
static bool IsApplicationExtension(const wchar_t* path) {
    const wchar_t* dot = PathFindExtensionW(path);
    if (!dot || !*dot) return false;
    static const wchar_t* const kAppExts[] = {
        L".exe", L".msi", L".msp", L".msc",
        L".bat", L".cmd", L".com",
        L".pif", L".scr",
        L".ps1", L".psm1", L".psd1",
        L".vbs", L".vbe", L".js",  L".jse",
        L".wsf", L".wsh",
        L".ico", L".cur", L".ani",
        L".lnk",   // shortcuts (app launchers in Start Menu etc.)
        nullptr
    };
    for (int k = 0; kAppExts[k]; ++k)
        if (_wcsicmp(dot, kAppExts[k]) == 0) return true;
    return false;
}

// Extracts the filename from a URL or local path (URL-decoded, last path segment,
// query string and fragment stripped).
// Returns empty string if nothing useful can be extracted.
static std::wstring FilenameFromUrl(const wchar_t* url) {
    if (!url || !url[0]) return {};

    // Work on a copy so we can strip query/fragment in-place
    std::wstring work(url);

    // Strip fragment (#...) and query (?...)
    auto frag = work.find(L'#');
    if (frag != std::wstring::npos) work.resize(frag);
    auto query = work.find(L'?');
    if (query != std::wstring::npos) work.resize(query);

    // Find last '/' (URLs) or '\' (local paths)
    size_t slash = work.rfind(L'/');
    size_t bslash = work.rfind(L'\\');
    size_t sep = (slash  == std::wstring::npos) ? bslash
               : (bslash == std::wstring::npos) ? slash
               : std::max(slash, bslash);
    const wchar_t* last = (sep == std::wstring::npos) ? work.c_str()
                                                       : work.c_str() + sep + 1;
    if (!*last) return {};

    // URL-decode the segment
    std::wstring encoded(last);
    wchar_t decoded[MAX_PATH * 2] = {};
    DWORD len = static_cast<DWORD>(_countof(decoded));
    if (SUCCEEDED(UrlUnescapeW(encoded.data(), decoded, &len, 0)) && decoded[0])
        return decoded;
    return encoded;
}

// ---------------------------------------------------------------------------
// Parses a single .customDestinations-ms file and appends resolved file paths.
// The format is a plain binary stream containing embedded .lnk structures.
// Each .lnk starts with the shell link signature: 4C 00 00 00 01 14 02 00 00 ...
// Files (not directories) with valid paths on disk, and SharePoint/OneDrive
// documents stored as Office-app targets with URL arguments, are added to 'out'.
// Duplicate paths (case-insensitive) are skipped via the 'seen' set.
// fileMtime is used as the timestamp for online documents (no local mtime).
// useRankTimestamp: when true, fileMtime is a rank-based synthetic timestamp from the
// DestList and should be used directly, overriding the LNK header timestamps.
static void ParseCustomDestFile(
    const std::vector<uint8_t>& data,
    const FILETIME& fileMtime,
    std::vector<FileItem>& out,
    std::unordered_set<std::wstring>& seen,
    bool useRankTimestamp = false)
{
    static constexpr uint8_t kLnkSig[8] = {0x4C, 0x00, 0x00, 0x00,
                                             0x01, 0x14, 0x02, 0x00};
    const size_t sz = data.size();
    if (sz < 8) return;

    for (size_t i = 0; i + 8 <= sz; i++) {
        if (memcmp(&data[i], kLnkSig, 8) != 0) continue;

        // Read the LNK header timestamps directly from the binary.
        // MS-SHLLINK spec:
        //   AccessTime (offset 0x24 = 36): when the target was last accessed/opened
        //   WriteTime  (offset 0x2C = 44): when the target was last modified
        // Use AccessTime (offset 36, when the file was last opened) as the primary timestamp.
        // If AccessTime is zero (common for online-only docs), fall back to:
        //   1. fileMtime — which is either a DestList rank-based synthetic timestamp (when
        //      useRankTimestamp=true) or the autodest file's own mtime.
        // This ensures items with real open-times beat the rank-based approximation.
        FILETIME lnkAccessTime = fileMtime; // fallback
        if (i + 36 + 8 <= sz) {
            FILETIME at = {};
            memcpy(&at, &data[i + 36], sizeof(FILETIME));
            if (at.dwLowDateTime != 0 || at.dwHighDateTime != 0) {
                // Valid AccessTime: use it (reflects actual last-opened time)
                lnkAccessTime = at;
            } else if (!useRankTimestamp) {
                // No AccessTime and no rank timestamp: try WriteTime as fallback
                if (i + 44 + 8 <= sz)
                    memcpy(&lnkAccessTime, &data[i + 44], sizeof(FILETIME));
                if (lnkAccessTime.dwLowDateTime == 0 && lnkAccessTime.dwHighDateTime == 0)
                    lnkAccessTime = fileMtime;
            }
            // else: AccessTime=0 + useRankTimestamp → keep fileMtime (rank-based)
        }

        IStream* pStream = SHCreateMemStream(
            data.data() + i, static_cast<UINT>(sz - i));
        if (!pStream) continue;

        IShellLinkW* pLink = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr,
            CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&pLink);
        if (SUCCEEDED(hr)) {
            IPersistStream* pPS = nullptr;
            if (SUCCEEDED(pLink->QueryInterface(IID_IPersistStream, (void**)&pPS))) {
                if (SUCCEEDED(pPS->Load(pStream))) {

                    // --- Get the target path ---
                    wchar_t rawPath[MAX_PATH] = {};
                    pLink->GetPath(rawPath, MAX_PATH, nullptr, SLGP_RAWPATH);
                    wchar_t target[MAX_PATH] = {};
                    if (rawPath[0])
                        ExpandEnvironmentStringsW(rawPath, target, MAX_PATH);

                    std::wstring docPath;
                    std::wstring docName;
                    bool isOnline = false;

                    if (target[0] && !IsApplicationExtension(target)) {
                        // Direct file target (local path)
                        docPath = target;
                    } else if (IsApplicationExtension(target) || target[0] == L'\0') {
                        // App target (e.g. WINWORD.EXE) — look for document URL in arguments.
                        // This is how Office stores SharePoint / OneDrive online documents.
                        wchar_t args[1024] = {};
                        if (SUCCEEDED(pLink->GetArguments(args, _countof(args))) && args[0]) {
                            // Trim leading whitespace/quotes
                            const wchar_t* a = args;
                            while (*a == L' ' || *a == L'"') ++a;
                            if (_wcsnicmp(a, L"https://", 8) == 0 ||
                                _wcsnicmp(a, L"http://",  7) == 0)
                            {
                                docPath  = a;
                                isOnline = true;
                                // Use the Description field which stores the human-readable
                                // (URL-decoded) path — extract just the filename from it
                                wchar_t desc[2048] = {};
                                pLink->GetDescription(desc, _countof(desc));
                                if (desc[0])
                                    docName = FilenameFromUrl(desc);
                                if (docName.empty())
                                    docName = FilenameFromUrl(a);
                            }
                        }
                    }

                    if (!docPath.empty() && !IsApplicationExtension(docPath.c_str())) {
                        // Case-insensitive dedup
                        std::wstring key = docPath;
                        CharLowerW(key.data());
                        if (seen.insert(key).second) {
                            bool include = false;
                            FILETIME writeTime = fileMtime;
                            FILETIME createTime = fileMtime;

                            if (isOnline) {
                                // Online document — accept without local existence check.
                                // For online files, LNK AccessTime = server document modification
                                // date, NOT when the user last opened the file. Use the rank-based
                                // synthetic timestamp from DestList instead (when available).
                                include    = true;
                                writeTime  = useRankTimestamp ? fileMtime : lnkAccessTime;
                                createTime = writeTime;
                            } else {
                                DWORD attr = GetFileAttributesW(docPath.c_str());
                                if (attr != INVALID_FILE_ATTRIBUTES
                                    && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                                {
                                    include = true;
                                    WIN32_FILE_ATTRIBUTE_DATA fad = {};
                                    if (GetFileAttributesExW(docPath.c_str(),
                                            GetFileExInfoStandard, &fad)) {
                                        writeTime  = fad.ftLastWriteTime;
                                        createTime = fad.ftCreationTime;
                                    }
                                }
                            }

                            if (include) {
                                FileItem item;
                                item.fullPath    = docPath;
                                item.isDirectory = false;
                                item.lastWriteTime = writeTime;
                                item.creationTime  = createTime;

                                if (!docName.empty()) {
                                    item.name = docName;
                                } else if (isOnline) {
                                    item.name = FilenameFromUrl(docPath.c_str());
                                    if (item.name.empty()) item.name = docPath;
                                } else {
                                    auto sep = docPath.rfind(L'\\');
                                    item.name = (sep != std::wstring::npos)
                                        ? docPath.substr(sep + 1) : docPath;
                                }

                                if (isOnline && !item.name.empty())
                                    item.targetPath = item.name;

                                out.push_back(std::move(item));
                            }
                        }
                    }
                }
                pPS->Release();
            }
            pLink->Release();
        }
        pStream->Release();
    }
}

// Scans all .customDestinations-ms files under
// %APPDATA%\Microsoft\Windows\Recent\CustomDestinations\ and adds items to out/seen.
static void ScanCustomDestinations(
    int maxItems,
    std::vector<FileItem>& out,
    std::unordered_set<std::wstring>& seen)
{
    wchar_t appData[MAX_PATH] = {};
    if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr,
                         SHGFP_TYPE_CURRENT, appData) != S_OK)
        return;

    std::wstring folder = std::wstring(appData)
        + L"\\Microsoft\\Windows\\Recent\\CustomDestinations";

    // Collect all .customDestinations-ms files, sorted by modification time desc
    // (most recently active app first) so we fill the list with fresher docs.
    struct CdFile { std::wstring path; FILETIME mtime; };
    std::vector<CdFile> cdFiles;
    {
        WIN32_FIND_DATAW fd = {};
        HANDLE hFind = FindFirstFileW(
            (folder + L"\\*.customDestinations-ms").c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            // Skip temp files written while an app is updating the list
            if (wcsstr(fd.cFileName, L"~RF") != nullptr) continue;
            cdFiles.push_back({ folder + L"\\" + fd.cFileName,
                                 fd.ftLastWriteTime });
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    // Sort files: most recently modified app first
    std::sort(cdFiles.begin(), cdFiles.end(),
        [](const CdFile& a, const CdFile& b) {
            return CompareFileTime(&a.mtime, &b.mtime) > 0;
        });

    for (const auto& cf : cdFiles) {
        // Read file into memory
        HANDLE hFile = CreateFileW(cf.path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) continue;

        DWORD fileSize = GetFileSize(hFile, nullptr);
        std::vector<uint8_t> data(fileSize);
        DWORD bytesRead = 0;
        ReadFile(hFile, data.data(), fileSize, &bytesRead, nullptr);
        CloseHandle(hFile);
        if (bytesRead < 8) continue;
        data.resize(bytesRead);

        ParseCustomDestFile(data, cf.mtime, out, seen);

        // Stop reading more files once we have enough candidates
        if ((int)out.size() >= maxItems * 3) break;
    }
}

// Parses one .automaticDestinations-ms file (OLE Compound Document / CFB format).
// Opens via IStorage, reads the DestList stream to get per-item access timestamps,
// then reads each numbered hex-named stream as a serialized LNK.
// Uses raw CFB binary parsing (no IStorage COM dependency).
static void ParseAutoDestFile(
    const std::wstring& filePath,
    const FILETIME& fileMtime,
    std::vector<FileItem>& out,
    std::unordered_set<std::wstring>& seen)
{
    // --- Read entire file into memory -------------------------------------------
    HANDLE hf = CreateFileW(filePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return;

    DWORD fileSize = GetFileSize(hf, nullptr);
    std::vector<uint8_t> raw(fileSize);
    DWORD bytesRead = 0;
    ReadFile(hf, raw.data(), fileSize, &bytesRead, nullptr);
    CloseHandle(hf);
    if (bytesRead < 512) return;
    raw.resize(bytesRead);

    // Validate CFB magic: D0 CF 11 E0 A1 B1 1A E1
    static const uint8_t cfbMagic[8] = {0xD0,0xCF,0x11,0xE0,0xA1,0xB1,0x1A,0xE1};
    if (memcmp(raw.data(), cfbMagic, 8) != 0) return;

    // --- Parse CFB header -------------------------------------------------------
    USHORT ssPow       = *reinterpret_cast<const USHORT*>(raw.data() + 30); // sector size pow (9 → 512)
    USHORT msPow       = *reinterpret_cast<const USHORT*>(raw.data() + 32); // mini-sector size pow (6 → 64)
    DWORD  csectFAT    = *reinterpret_cast<const DWORD*>(raw.data() + 44);
    DWORD  dirStart    = *reinterpret_cast<const DWORD*>(raw.data() + 48);
    DWORD  miniCutoff  = *reinterpret_cast<const DWORD*>(raw.data() + 56); // 4096
    DWORD  miniFATSt   = *reinterpret_cast<const DWORD*>(raw.data() + 60); // first mini-FAT sector

    size_t ss  = (size_t)1 << ssPow;  // regular sector size (512)
    size_t mss = (size_t)1 << msPow;  // mini-sector size (64)
    if (ss == 0 || ss > 4096 || mss == 0 || mss > 512 || csectFAT == 0) return;

    // Collect FAT sector numbers from header DIFAT (up to 109)
    std::vector<DWORD> fatSectors;
    for (DWORD i = 0; i < 109 && fatSectors.size() < csectFAT; i++) {
        DWORD s = *reinterpret_cast<const DWORD*>(raw.data() + 76 + i * 4);
        if (s >= 0xFFFFFFFC) break;
        fatSectors.push_back(s);
    }

    auto sectorOffset = [&](DWORD sector) -> size_t {
        return (size_t)(sector + 1) * ss;
    };
    auto fatEntry = [&](DWORD sector) -> DWORD {
        size_t ePerFat = ss / 4;
        size_t wf = sector / ePerFat;
        size_t oi = (sector % ePerFat) * 4;
        if (wf >= fatSectors.size()) return 0xFFFFFFFF;
        size_t abs = sectorOffset(fatSectors[wf]) + oi;
        if (abs + 4 > raw.size()) return 0xFFFFFFFF;
        return *reinterpret_cast<const DWORD*>(raw.data() + abs);
    };

    // Read a regular (large) stream by following the FAT chain
    auto readRegularStream = [&](DWORD startSector, size_t streamSize) -> std::vector<uint8_t> {
        std::vector<uint8_t> result;
        result.reserve(streamSize > 0 ? streamSize : ss);
        DWORD s = startSector;
        for (int g = 0; g < 5000 && s < 0xFFFFFFFE; g++) {
            size_t off = sectorOffset(s);
            size_t end = off + ss;
            if (off >= raw.size()) break;
            if (end > raw.size()) end = raw.size();
            result.insert(result.end(), raw.begin() + off, raw.begin() + end);
            s = fatEntry(s);
        }
        if (streamSize > 0 && result.size() > streamSize) result.resize(streamSize);
        return result;
    };

    // --- Parse directory entries (128 bytes each) --------------------------------
    auto readDirEntries = [&]() -> std::vector<std::vector<uint8_t>> {
        std::vector<std::vector<uint8_t>> entries;
        DWORD s = dirStart;
        for (int g = 0; g < 1000 && s < 0xFFFFFFFE; g++) {
            size_t off = sectorOffset(s);
            for (size_t i = 0; i + 128 <= ss && off + i + 128 <= raw.size(); i += 128)
                entries.push_back({ raw.begin() + off + i, raw.begin() + off + i + 128 });
            s = fatEntry(s);
        }
        return entries;
    };
    auto dirEntries = readDirEntries();

    // Helpers to read fields from a 128-byte directory entry
    auto deName = [](const std::vector<uint8_t>& e) -> std::wstring {
        if (e.size() < 66) return {};
        USHORT nl = *reinterpret_cast<const USHORT*>(e.data() + 64); // bytes incl. NUL
        if (nl < 2 || nl > 64) return {};
        return std::wstring(reinterpret_cast<const wchar_t*>(e.data()), (nl - 2) / 2);
    };
    auto deType  = [](const std::vector<uint8_t>& e) -> uint8_t { return e.size() >= 128 ? e[66] : 0; };
    auto deSect  = [](const std::vector<uint8_t>& e) -> DWORD {
        return *reinterpret_cast<const DWORD*>(e.data() + 116);
    };
    auto deSize  = [](const std::vector<uint8_t>& e) -> size_t {
        return (size_t)*reinterpret_cast<const DWORD*>(e.data() + 120);
    };

    // --- Build mini-stream reader ------------------------------------------------
    // Small streams (size < miniCutoff) are stored in the mini-stream container,
    // which is the root entry's (type=5) data stream in the regular FAT.
    // Mini-FAT maps mini-sectors (each mss bytes) within that container.
    std::vector<uint8_t> miniContainer;
    std::vector<DWORD>   miniFAT;

    for (const auto& de : dirEntries) {
        if (deType(de) == 5) { // root entry
            miniContainer = readRegularStream(deSect(de), deSize(de));
            break;
        }
    }
    // Read mini-FAT sectors into a flat vector
    {
        DWORD s = miniFATSt;
        for (int g = 0; g < 1000 && s < 0xFFFFFFFE; g++) {
            size_t off = sectorOffset(s);
            size_t end = off + ss;
            if (off >= raw.size()) break;
            if (end > raw.size()) end = raw.size();
            for (size_t i = off; i + 4 <= end; i += 4)
                miniFAT.push_back(*reinterpret_cast<const DWORD*>(raw.data() + i));
            s = fatEntry(s);
        }
    }
    auto miniFATEntry = [&](DWORD ms) -> DWORD {
        return ms < miniFAT.size() ? miniFAT[ms] : 0xFFFFFFFF;
    };

    // Read a stream from the mini-stream
    auto readMiniStream = [&](DWORD startMs, size_t streamSize) -> std::vector<uint8_t> {
        std::vector<uint8_t> result;
        result.reserve(streamSize > 0 ? streamSize : mss);
        DWORD ms = startMs;
        for (int g = 0; g < 10000 && ms < 0xFFFFFFFE; g++) {
            size_t off = (size_t)ms * mss;
            size_t end = off + mss;
            if (off >= miniContainer.size()) break;
            if (end > miniContainer.size()) end = miniContainer.size();
            result.insert(result.end(), miniContainer.begin() + off, miniContainer.begin() + end);
            ms = miniFATEntry(ms);
        }
        if (streamSize > 0 && result.size() > streamSize) result.resize(streamSize);
        return result;
    };

    // Auto-select regular vs mini stream based on size vs cutoff
    auto readStream = [&](DWORD startSector, size_t streamSize) -> std::vector<uint8_t> {
        if (streamSize > 0 && streamSize < (size_t)miniCutoff && !miniContainer.empty())
            return readMiniStream(startSector, streamSize);
        return readRegularStream(startSector, streamSize);
    };

    // --- Step 1: read DestList for recency ordering ----------------------------
    // DestList entry offsets (from forensic research):
    //   v<=3 (Win7/8.1): fixed=88,  idOff=56, atOff=64, nlOff=86
    //   v4-5 (Win10):    fixed=130, idOff=72, atOff=80, nlOff=120
    //   v6+  (Win10 21H2+): fixed=130, idOff=88, atOff=0 (zero), nlOff=128
    //
    // FILETIMEs are zero for SharePoint/online files, so we sort by RANK
    // (entry position in DestList, 0=most recent) using synthetic timestamps.
    struct DestEntry { DWORD rank; FILETIME time; };
    std::unordered_map<DWORD, DestEntry> entryMap;

    for (const auto& de : dirEntries) {
        if (deType(de) != 2) continue;
        if (deName(de) != L"DestList") continue;

        // DestList is always large (> miniCutoff), so use regular stream
        auto dlBuf = readRegularStream(deSect(de), deSize(de));
        if (dlBuf.size() < 32) break;

        DWORD version  = *reinterpret_cast<const DWORD*>(dlBuf.data());
        DWORD nEntries = *reinterpret_cast<const DWORD*>(dlBuf.data() + 4);

        // Base offsets for v4-5. v6 uses per-entry detection (see loop below).
        size_t fixed = 130, idOff = 72, atOff = 80, nlOff = 120;
        if (version < 4) { fixed = 88; idOff = 56; atOff = 64; nlOff = 86; }

        ULONGLONG baseFt = ((ULONGLONG)fileMtime.dwHighDateTime << 32)
                          | fileMtime.dwLowDateTime;
        size_t pos = 32;
        for (DWORD i = 0; i < nEntries; i++) {
            if (pos + fixed > dlBuf.size()) break;

            size_t entFixed = fixed;
            DWORD  eid      = 0;
            WORD   nameLen  = 0;
            FILETIME at     = {};

            if (version >= 6) {
                // v6 entries come in two sizes: 130 or 134 bytes fixed.
                // The 134-byte variant has 4 extra bytes before nameLen.
                // Detect by checking whether nameLen at offset 128 is plausible.
                WORD nl128 = (pos + 130 <= dlBuf.size())
                    ? *reinterpret_cast<const WORD*>(dlBuf.data() + pos + 128) : 0;
                WORD nl132 = (pos + 134 <= dlBuf.size())
                    ? *reinterpret_cast<const WORD*>(dlBuf.data() + pos + 132) : 0;
                if (nl128 == 0 && nl132 > 0 && nl132 < 4096) {
                    entFixed = 134;
                    eid      = *reinterpret_cast<const DWORD*>(dlBuf.data() + pos + 92);
                    nameLen  = nl132;
                } else {
                    entFixed = 130;
                    eid      = *reinterpret_cast<const DWORD*>(dlBuf.data() + pos + 88);
                    nameLen  = nl128;
                }
            } else {
                eid = *reinterpret_cast<const DWORD*>(dlBuf.data() + pos + idOff);
                if (atOff > 0 && pos + atOff + 8 <= dlBuf.size())
                    memcpy(&at, dlBuf.data() + pos + atOff, sizeof(FILETIME));
                nameLen = (pos + nlOff + 2 <= dlBuf.size())
                    ? *reinterpret_cast<const WORD*>(dlBuf.data() + pos + nlOff) : 0;
            }
            if (nameLen >= 4096) nameLen = 0; // guard against corruption

            DestEntry dentry;
            dentry.rank = i;
            if (at.dwHighDateTime != 0 || at.dwLowDateTime != 0) {
                dentry.time = at;
            } else {
                ULONGLONG t = (baseFt > (ULONGLONG)i * 216000000000ULL)
                              ? baseFt - (ULONGLONG)i * 216000000000ULL : 0;
                dentry.time.dwLowDateTime  = (DWORD)(t & 0xFFFFFFFF);
                dentry.time.dwHighDateTime = (DWORD)(t >> 32);
            }
            if (eid != 0) entryMap[eid] = dentry;
            pos += entFixed + (size_t)nameLen * 2;
        }
        break;
    }

    // --- Step 2: read each hex-named stream as a serialized LNK ---------------
    for (const auto& de : dirEntries) {
        if (deType(de) != 2) continue;
        std::wstring name = deName(de);
        if (name.empty()) continue;

        bool isHex = true;
        for (wchar_t c : name) if (!iswxdigit(c)) { isHex = false; break; }
        if (!isHex) continue;

        DWORD eid = (DWORD)wcstoul(name.c_str(), nullptr, 16);
        FILETIME streamTime = fileMtime;
        auto it = entryMap.find(eid);
        if (it != entryMap.end()) streamTime = it->second.time;

        size_t sz = deSize(de);
        if (sz < 8 || sz > 100 * 1024) continue;

        auto lnkData = readStream(deSect(de), sz);
        if (lnkData.size() >= 8) {
            bool hasRank = (entryMap.find(eid) != entryMap.end());
            ParseCustomDestFile(lnkData, streamTime, out, seen, hasRank);
        }
    }
}

// Scans all .automaticDestinations-ms files and populates out/seen.
static void ScanAutomaticDestinations(
    int maxItems,
    std::vector<FileItem>& out,
    std::unordered_set<std::wstring>& seen)
{
    wchar_t appData[MAX_PATH] = {};
    if (!SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr,
                                    SHGFP_TYPE_CURRENT, appData))) return;

    std::wstring folder = std::wstring(appData)
        + L"\\Microsoft\\Windows\\Recent\\AutomaticDestinations";

    struct AdFile { std::wstring path; FILETIME mtime; };
    std::vector<AdFile> adFiles;
    {
        WIN32_FIND_DATAW fd = {};
        HANDLE hFind = FindFirstFileW(
            (folder + L"\\*.automaticDestinations-ms").c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (wcsstr(fd.cFileName, L"~RF") != nullptr) continue;
            adFiles.push_back({ folder + L"\\" + fd.cFileName, fd.ftLastWriteTime });
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }

    std::sort(adFiles.begin(), adFiles.end(),
        [](const AdFile& a, const AdFile& b) {
            return CompareFileTime(&a.mtime, &b.mtime) > 0;
        });

    for (const auto& af : adFiles) {
        size_t prevSize = out.size();
        ParseAutoDestFile(af.path, af.mtime, out, seen);
        // Cap this app's contribution so one busy app can't fill all slots
        if (out.size() - prevSize > (size_t)maxItems) {
            std::sort(out.begin() + prevSize, out.end(),
                [](const FileItem& a, const FileItem& b) {
                    return CompareFileTime(&a.lastWriteTime, &b.lastWriteTime) > 0;
                });
            out.resize(prevSize + maxItems);
        }
    }
}

// Retrieves recently accessed documents by scanning the Windows Recent folder
// with full .lnk resolution, always filtering out directories.
static std::vector<FileItem> ScanRecentDocs(int maxItems, ConfigData::SortMode sortMode) {
    std::vector<FileItem> items;
    items.reserve(maxItems * 4);
    std::unordered_set<std::wstring> seen;

    // AutomaticDestinations carry real per-item access timestamps via DestList.
    ScanAutomaticDestinations(maxItems, items, seen);
    // CustomDestinations may have pinned/manual items not in AutomaticDestinations.
    ScanCustomDestinations(maxItems, items, seen);

    auto cmpFt = [](const FILETIME& a, const FILETIME& b) {
        return CompareFileTime(&a, &b) > 0;
    };
    switch (sortMode) {
    case ConfigData::SortMode::DateModifiedAsc:
        std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
            return CompareFileTime(&a.lastWriteTime, &b.lastWriteTime) < 0; });
        break;
    case ConfigData::SortMode::DateCreatedDesc:
        std::sort(items.begin(), items.end(), [&](const FileItem& a, const FileItem& b) {
            return CompareFileTime(&a.creationTime, &b.creationTime) > 0; });
        break;
    case ConfigData::SortMode::DateCreatedAsc:
        std::sort(items.begin(), items.end(), [&](const FileItem& a, const FileItem& b) {
            return CompareFileTime(&a.creationTime, &b.creationTime) < 0; });
        break;
    case ConfigData::SortMode::NameAsc:
        std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
            return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0; });
        break;
    case ConfigData::SortMode::NameDesc:
        std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
            return _wcsicmp(a.name.c_str(), b.name.c_str()) > 0; });
        break;
    default:  // DateModifiedDesc
        std::sort(items.begin(), items.end(), [&](const FileItem& a, const FileItem& b) {
            return cmpFt(a.lastWriteTime, b.lastWriteTime); });
        break;
    }

    if ((int)items.size() > maxItems) items.resize(maxItems);

    return items;
}

std::vector<FileItem> FileService::ScanFolder(const std::wstring& folderPath, int maxItems, bool includeDirs, const std::wstring& filterRegex, ConfigData::SortMode sortMode, bool resolveLnk) {
    std::vector<FileItem> items;
    if (folderPath.empty()) return items;

    // Special mode: scan the Windows Recent folder with full .lnk resolution, files only
    if (folderPath == L"::RecentDocs::")
        return ScanRecentDocs(maxItems, sortMode);

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

        // Resolve .lnk shortcuts — use target for dir-check, extension, and thumbnails
        if (resolveLnk && !item.isDirectory) {
            auto dot = item.name.rfind(L'.');
            if (dot != std::wstring::npos) {
                std::wstring ext = item.name.substr(dot);
                for (auto& c : ext) c = (wchar_t)towlower(c);
                if (ext == L".lnk") {
                    bool targetIsDir = false;
                    std::wstring target = ResolveLnk(item.fullPath, targetIsDir);
                    if (target.empty()) continue; // skip unresolvable / dead shortcuts
                    item.targetPath  = target;
                    item.isDirectory = targetIsDir;
                }
            }
        }

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
        L".bmp", L".tiff", L".tif"
        // .svg is handled by lunasvg via GetSvgThumbnail
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

bool FileService::IsSvgExtension(const std::wstring& path) {
    const wchar_t* dot = wcsrchr(path.c_str(), L'.');
    if (!dot) return false;
    wchar_t ext[8] = {};
    size_t i = 0;
    for (const wchar_t* p = dot; *p && i < 7; ++p, ++i)
        ext[i] = (wchar_t)towlower(*p);
    return wcscmp(ext, L".svg") == 0 || wcscmp(ext, L".svgz") == 0;
}

HBITMAP FileService::GetSvgThumbnail(const std::wstring& path, int size) {
    // Read file with Windows APIs (handles Unicode paths), then pass to lunasvg
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return nullptr;

    LARGE_INTEGER fileSize = {};
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0 || fileSize.QuadPart > 64 * 1024 * 1024) {
        CloseHandle(hFile);
        return nullptr;
    }

    std::string svgData((size_t)fileSize.QuadPart, '\0');
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, svgData.data(), (DWORD)fileSize.QuadPart, &bytesRead, nullptr);
    CloseHandle(hFile);
    if (!ok || bytesRead != (DWORD)fileSize.QuadPart) return nullptr;

    auto doc = lunasvg::Document::loadFromData(svgData);
    if (!doc) return nullptr;

    float svgW = doc->width();
    float svgH = doc->height();
    int dstW = size, dstH = size;
    if (svgW > 0 && svgH > 0) {
        float scale = std::min((float)size / svgW, (float)size / svgH);
        dstW = std::max(1, (int)(svgW * scale));
        dstH = std::max(1, (int)(svgH * scale));
    }

    auto bitmap = doc->renderToBitmap(dstW, dstH);
    if (bitmap.isNull()) return nullptr;

    BITMAPINFOHEADER bih = {};
    bih.biSize        = sizeof(bih);
    bih.biWidth       = size;
    bih.biHeight      = -size;
    bih.biPlanes      = 1;
    bih.biBitCount    = 32;
    bih.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, (BITMAPINFO*)&bih, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!hBmp || !bits) return nullptr;

    ZeroMemory(bits, size * size * 4);

    int offX = (size - dstW) / 2;
    int offY = (size - dstH) / 2;

    // lunasvg ARGB32_Premultiplied in little-endian memory = BGRA bytes → matches Windows DIB
    const uint8_t* src = bitmap.data();
    int srcStride = bitmap.stride();
    uint8_t* dst = (uint8_t*)bits;
    for (int y = 0; y < dstH; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        uint8_t* dstRow = dst + ((offY + y) * size + offX) * 4;
        memcpy(dstRow, srcRow, dstW * 4);
    }

    return hBmp;
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

HICON FileService::GetShellIconByExtension(const std::wstring& nameWithExt) {
    // SHGFI_USEFILEATTRIBUTES tells SHGetFileInfoW to resolve the icon purely
    // from the extension — the file does not need to exist on disk.
    SHFILEINFOW sfi = {};
    DWORD_PTR res = SHGetFileInfoW(nameWithExt.c_str(),
                                   FILE_ATTRIBUTE_NORMAL,
                                   &sfi, sizeof(sfi),
                                   SHGFI_SYSICONINDEX | SHGFI_ICON | SHGFI_LARGEICON
                                   | SHGFI_USEFILEATTRIBUTES);
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

// Like GetShellBitmap but resolves the icon by file extension only (file need
// not exist on disk). Uses GetShellIconByExtension (SHGFI_USEFILEATTRIBUTES) to
// get the HICON reliably, then DrawIconEx into a raw 32-bpp DIB section at native
// size to capture per-pixel alpha correctly. Avoids Gdiplus::Bitmap::FromHICON
// which returns alpha=0 for system image list icons. Appends a cloud badge overlay.
HBITMAP FileService::GetShellBitmapByExtension(const std::wstring& nameWithExt, int size) {
    HICON hIcon = GetShellIconByExtension(nameWithExt);
    if (!hIcon) return nullptr;

    // --- Step 1: DrawIconEx into a 256x256 32-bpp DIB section ---------------------
    // Use 256 (SHIL_JUMBO native size) for best quality, then scale down with Gdiplus.
    const int kNative = 256;

    BITMAPINFOHEADER bihSrc = {};
    bihSrc.biSize        = sizeof(bihSrc);
    bihSrc.biWidth       = kNative;
    bihSrc.biHeight      = -kNative; // top-down
    bihSrc.biPlanes      = 1;
    bihSrc.biBitCount    = 32;
    bihSrc.biCompression = BI_RGB;

    void* srcBits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HBITMAP hSrcBmp = CreateDIBSection(hdcScreen, (BITMAPINFO*)&bihSrc,
                                       DIB_RGB_COLORS, &srcBits, nullptr, 0);
    ReleaseDC(nullptr, hdcScreen);
    if (!hSrcBmp) { DestroyIcon(hIcon); return nullptr; }

    // Zero-init: background = transparent black (alpha=0)
    memset(srcBits, 0, (size_t)kNative * kNative * 4);

    {
        HDC hdc  = CreateCompatibleDC(nullptr);
        HBITMAP hOld = (HBITMAP)SelectObject(hdc, hSrcBmp);
        // DI_NORMAL: for 32-bpp icons (PNG-based) DrawIconEx writes per-pixel alpha.
        // For mask-based icons alpha stays 0 -- handled below.
        DrawIconEx(hdc, 0, 0, hIcon, kNative, kNative, 0, nullptr, DI_NORMAL);
        SelectObject(hdc, hOld);
        DeleteDC(hdc);
    }
    DestroyIcon(hIcon);

    // Read raw pixel data directly from the DIB section pointer (no GetDIBits needed)
    const DWORD* raw = static_cast<const DWORD*>(srcBits);
    bool hasAlpha = false;
    for (int i = 0; i < kNative * kNative; i++)
        if ((raw[i] >> 24) != 0) { hasAlpha = true; break; }

    std::vector<DWORD> px((size_t)kNative * kNative);
    memcpy(px.data(), raw, px.size() * 4);
    DeleteObject(hSrcBmp);

    // For mask-based icons (no per-pixel alpha), mark all pixels opaque.
    if (!hasAlpha)
        for (auto& p : px) p |= 0xFF000000u;

    // --- Step 2: wrap in Gdiplus, scale to target size ----------------------------
    Gdiplus::Bitmap nativeBmp(kNative, kNative, PixelFormat32bppARGB);
    if (nativeBmp.GetLastStatus() != Gdiplus::Ok) return nullptr;
    {
        Gdiplus::Rect r0(0, 0, kNative, kNative);
        Gdiplus::BitmapData bd = {};
        if (nativeBmp.LockBits(&r0, Gdiplus::ImageLockModeWrite,
                               PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
            // DrawIconEx into a top-down DIB stores row 0 = top of icon.
            // However GDI draws into DIBs in bottom-up fashion when biHeight > 0;
            // with biHeight < 0 (top-down) the first pixel in memory IS the top row,
            // but GDI's coordinate origin is still bottom-left, causing a vertical flip.
            // Reverse row order to compensate.
            for (int row = 0; row < kNative; row++)
                memcpy((BYTE*)bd.Scan0 + row * bd.Stride,
                       px.data() + (size_t)(kNative - 1 - row) * kNative, (size_t)kNative * 4);
            nativeBmp.UnlockBits(&bd);
        }
    }

    // Scale to size x size with high-quality bicubic + cloud badge
    Gdiplus::Bitmap iconBmp(size, size, PixelFormat32bppARGB);
    if (iconBmp.GetLastStatus() != Gdiplus::Ok) return nullptr;
    {
        Gdiplus::Graphics g(&iconBmp);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.DrawImage(&nativeBmp, 0, 0, size, size);

        // --- Cloud badge overlay (bottom-left corner) ---
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        const float r  = size * 0.22f;
        const float cx = r * 0.9f;
        const float cy = (float)size - r * 0.9f;

        // Load cloud SVG once. Use the OneDrive cloud path with blue fill.
        // The path coordinates are in a 512x512 viewBox (y increases downward in SVG).
        // transform="scale(1,-1) translate(0,-512)" flips it vertically so the bumps
        // face upward as expected.
        static std::string sCloudSvgData;
        static bool sCloudSvgLoaded = false;
        if (!sCloudSvgLoaded) {
            sCloudSvgLoaded = true;
            wchar_t userProfile[MAX_PATH] = {};
            if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
                std::wstring svgPath = std::wstring(userProfile)
                    + L"\\Downloads\\cloud, microsoft, onedrive svg icon.svg";
                FILE* f = _wfopen(svgPath.c_str(), L"rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long sz2 = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    if (sz2 > 0) {
                        sCloudSvgData.resize((size_t)sz2);
                        fread(sCloudSvgData.data(), 1, (size_t)sz2, f);
                    }
                    fclose(f);
                }
            }
        }
        // Build the final SVG: the cloud path with explicit blue fill + vertical flip.
        // We use only the cloud outline path regardless of whether the file was loaded,
        // because the file has no fill attribute (would render black) and has a complex
        // polka-dot pattern that doesn't suit a small badge.
        static const char kCloudSvg[] =
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 512 512'>"
            "<path fill='#0078D4' transform='scale(1,-1) translate(0,-512)' d='"
            "M466,336.292c0,24.738-20.052,44.783-44.783,44.783H194.621"
            "c-30.063,0-54.43-24.366-54.43-54.43c0-30.062,24.367-54.429,54.43-54.429"
            "c2.126,0,4.208,0.144,6.266,0.379c0.927-40.431,33.944-72.954,74.61-72.954"
            "c25.795,0,48.535,13.071,61.957,32.964c8.157-4.01,17.311-6.327,27.018-6.327"
            "c33.898,0,61.379,27.474,61.379,61.38c0,1.375-0.114,2.719-0.205,4.078"
            "C448.295,293.962,466,313.058,466,336.292z"
            "M187.086,257.178c8.112-41.396,45.011-72.979,88.411-72.979"
            "c25.779,0,50.039,10.976,67.023,29.858c7.102-2.142,14.454-3.221,21.951-3.221"
            "c0.045,0,0.083,0,0.121,0c-5.043-44.958-43.15-79.912-89.444-79.912"
            "c-35.084,0-65.413,20.105-80.277,49.386c-9.434-5.362-20.303-8.469-31.932-8.469"
            "c-35.737,0-64.713,28.977-64.713,64.721c0,5.432,0.744,10.672,2.005,15.708"
            "C70.062,253.66,46,278.482,46,309.001c0,31.4,25.46,56.86,56.86,56.86h33.975"
            "c-7.618-11.188-12.084-24.693-12.084-39.216C124.75,290.658,152.094,260.944,187.086,257.178z"
            "'/></svg>";

        auto cloudDoc = lunasvg::Document::loadFromData(kCloudSvg, sizeof(kCloudSvg) - 1);
        if (cloudDoc) {
            int badgePx = static_cast<int>(r * 2.0f + 0.5f);
            auto bm2 = cloudDoc->renderToBitmap(badgePx, badgePx);
            if (!bm2.isNull()) {
                Gdiplus::Bitmap svgBmp(badgePx, badgePx, badgePx * 4,
                                      PixelFormat32bppPARGB,
                                      const_cast<uint8_t*>(bm2.data()));
                g.DrawImage(&svgBmp,
                    Gdiplus::RectF(cx - r, cy - r, r * 2.0f, r * 2.0f),
                    0.0f, 0.0f, (float)badgePx, (float)badgePx,
                    Gdiplus::UnitPixel);
            }
        }
    }

    // --- Step 3: convert to premultiplied-alpha HBITMAP (required by UpdateLayeredWindow) ---
    BITMAPINFOHEADER bih = {};
    bih.biSize        = sizeof(bih);
    bih.biWidth       = size;
    bih.biHeight      = -size;
    bih.biPlanes      = 1;
    bih.biBitCount    = 32;
    bih.biCompression = BI_RGB;

    void* dstBits = nullptr;
    hdcScreen = GetDC(nullptr);
    HBITMAP hDst = CreateDIBSection(hdcScreen, (BITMAPINFO*)&bih,
                                    DIB_RGB_COLORS, &dstBits, nullptr, 0);
    ReleaseDC(nullptr, hdcScreen);
    if (!hDst || !dstBits) { if (hDst) DeleteObject(hDst); return nullptr; }

    {
        Gdiplus::Rect rect(0, 0, size, size);
        Gdiplus::BitmapData bd = {};
        if (iconBmp.LockBits(&rect, Gdiplus::ImageLockModeRead,
                             PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
            const DWORD* src = static_cast<const DWORD*>(bd.Scan0);
            DWORD*       dst = static_cast<DWORD*>(dstBits);
            for (int i = 0; i < size * size; ++i) {
                DWORD a  = (src[i] >> 24) & 0xFF;
                DWORD r2 = (src[i] >> 16) & 0xFF;
                DWORD gn = (src[i] >>  8) & 0xFF;
                DWORD b  = (src[i]      ) & 0xFF;
                r2 = r2 * a / 255;
                gn = gn * a / 255;
                b  = b  * a / 255;
                dst[i] = (a << 24) | (r2 << 16) | (gn << 8) | b;
            }
            iconBmp.UnlockBits(&bd);
        }
    }

    return hDst;
}
