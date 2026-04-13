// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#pragma once
#include "pch.h"
#include "Config.h"

struct FileItem {
    std::wstring name;
    std::wstring fullPath;
    std::wstring targetPath;  // resolved target if fullPath is a .lnk shortcut
    bool isDirectory = false;
    FILETIME lastWriteTime  = {};
    FILETIME creationTime   = {};
};

class FileService {
public:
    static std::vector<FileItem> ScanFolder(
        const std::wstring& folderPath,
        int maxItems = 15,
        bool includeDirs = true,
        const std::wstring& filterRegex = L"",
        ConfigData::SortMode sortMode = ConfigData::SortMode::DateModifiedDesc,
        bool resolveLnk = true,
        HWND hwndForAuth = nullptr);

    // Returns true when folderPath is the special "recent documents" sentinel
    static bool IsRecentDocsSentinel(const std::wstring& folderPath) {
        return folderPath == L"::RecentDocs::";
    }
    // Returns true when folderPath is the "Seneste" (Explorer Recent) sentinel
    static bool IsRecentFilesSentinel(const std::wstring& folderPath) {
        return folderPath == L"::RecentFiles::";
    }
    static bool IsVirtualSentinel(const std::wstring& folderPath) {
        return IsRecentDocsSentinel(folderPath) || IsRecentFilesSentinel(folderPath);
    }
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
