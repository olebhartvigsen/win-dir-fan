#pragma once
#include "pch.h"
#include "Config.h"

struct FileItem {
    std::wstring name;
    std::wstring fullPath;
    bool isDirectory = false;
    FILETIME lastWriteTime = {};
};

class FileService {
public:
    static std::vector<FileItem> ScanFolder(
        const std::wstring& folderPath,
        int maxItems = 15,
        bool includeDirs = true,
        const std::wstring& filterRegex = L"",
        ConfigData::SortMode sortMode = ConfigData::SortMode::DateModifiedDesc);
    static HBITMAP GetShellBitmap(const std::wstring& path, int size);
    static HICON   GetShellIcon(const std::wstring& path);
};
