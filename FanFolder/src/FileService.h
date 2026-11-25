// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#pragma once
#include "pch.h"
#include "Config.h"

struct FileItem {
    std::wstring name;
    std::wstring fullPath;
    std::wstring relativeDir;  // "" at root, "Sub\Deeper" for nested files
    std::wstring targetPath;  // resolved target if fullPath is a .lnk shortcut
    bool isDirectory = false;
    FILETIME lastWriteTime  = {};
    FILETIME creationTime   = {};
};

class FileService {
public:
    // Result of a type-to-filter search. items is ranked, sorted and already
    // truncated to the requested maximum; totalMatches counts every match the
    // walk found, so the UI can show "3 of 27".
    struct SearchResultSet {
        std::vector<FileItem> items;
        int  totalMatches    = 0;
        bool candidateCapHit = false;
    };

    static std::vector<FileItem> ScanFolder(
        const std::wstring& folderPath,
        int maxItems = 15,
        bool includeDirs = true,
        const std::wstring& filterRegex = L"",
        ConfigData::SortMode sortMode = ConfigData::SortMode::DateModifiedDesc,
        bool resolveLnk = true,
        bool includeSubfolderFiles = false);

    // Searches every item ScanFolder can reach, ignoring the maxItems window.
    // Dispatches on the virtual folder sentinels the same way ScanFolder does.
    // Pass the raw query: it is lowercased once and reused for every candidate.
    static SearchResultSet SearchFolder(
        const std::wstring& folderPath,
        const std::wstring& query,
        int maxResults,
        bool includeDirs,
        const std::wstring& filterRegex,
        ConfigData::SortMode sortMode,
        bool includeSubfolderFiles);

    // Ranking used by both the instant in-memory pass and the full search.
    // lowerQuery must already be lowercased.
    static int MatchScore(const FileItem& item, const std::wstring& lowerQuery);

    // Resolves a .lnk shortcut file to its target path.
    // Returns the target path, or empty if resolution fails or target doesn't exist.
    static std::wstring ResolveLnk(const std::wstring& lnkPath, bool& outIsDir);

    // Drops the in-memory name index used by SearchFolder, so the next search
    // rebuilds it. Call whenever the folder contents or the scan configuration
    // may have changed.
    static void InvalidateSearchIndex();

    // Builds the index ahead of time on the calling thread. The prewarm worker
    // calls this at startup and after every change, so neither the first fan
    // open nor the first keystroke pays for a tree walk.
    static void WarmSearchIndex(const std::wstring& folderPath, bool includeDirs,
                                bool includeSubfolderFiles);

    // Returns true when folderPath is the special "recent documents" sentinel
    static bool IsRecentDocsSentinel(const std::wstring& folderPath) {
        return folderPath == L"::RecentDocs::";
    }
    // Returns true when folderPath is the "Seneste" (Explorer Recent) sentinel
    static bool IsRecentFilesSentinel(const std::wstring& folderPath) {
        return folderPath == L"::RecentFiles::";
    }
    // Returns true when folderPath is the "Seneste Office 365" sentinel.
    // Listed separately because GraphRecent is a Jump-List view, NOT a
    // directory: there is no folder to timestamp, so callers must always
    // rescan it rather than trusting a cached scan.
    static bool IsGraphRecentSentinel(const std::wstring& folderPath) {
        return folderPath == L"::GraphRecent::";
    }
    static bool IsVirtualSentinel(const std::wstring& folderPath) {
        return IsRecentDocsSentinel(folderPath) ||
               IsRecentFilesSentinel(folderPath) ||
               IsGraphRecentSentinel(folderPath);
    }
    // Reads the folder's last-write timestamp into outStamp.  A directory's
    // mtime moves when an entry is added, removed, renamed, or moved in or
    // out of it, so an unchanged value means a rescan would yield the same
    // items as the cached scan.
    // Returns false for a virtual sentinel, a non-directory, or an unreadable
    // path — callers MUST treat that as "always rescan" rather than "skip".
    static bool ReadFolderStamp(const std::wstring& folderPath, FILETIME& outStamp,
                                bool recursive = false);
    static HBITMAP GetShellBitmap(const std::wstring& path, int size);
    static HBITMAP GetShellThumbnail(const std::wstring& path, int size);
    static HBITMAP GetImageThumbnail(const std::wstring& path, int size);
    static HBITMAP GetSvgThumbnail(const std::wstring& path, int size);
    static HICON   GetShellIcon(const std::wstring& path);
    // Gets the icon for a file type by extension only (file need not exist).
    // Pass a filename like L"document.docx" or just an extension like L".docx".
    static HICON   GetShellIconByExtension(const std::wstring& nameWithExt);
    // Like GetShellBitmap but resolves icon by extension only (file need not exist).
    static HBITMAP GetShellBitmapByExtension(const std::wstring& nameWithExt, int size);

    // GDI+ natively handles these formats → actual image thumbnail
    static bool IsGdiImageExtension(const std::wstring& path);
    // Shell renders actual content for these (WebP via OS codec)
    static bool IsShellThumbnailExtension(const std::wstring& path);
    // SVG/SVGZ — rendered via lunasvg (more reliable than Windows shell handler)
    static bool IsSvgExtension(const std::wstring& path);
};
