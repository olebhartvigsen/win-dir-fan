#pragma once
#include "pch.h"

struct ConfigData {
    std::wstring folderPath;
    int  maxItems    = 15;
    bool includeDirs = true;
    std::wstring filterRegex;
    enum class SortMode { DateModifiedDesc, DateModifiedAsc, NameAsc, NameDesc }
        sortMode = SortMode::DateModifiedDesc;
    enum class AnimStyle { Fan, Glide, Spring, None }
        animStyle = AnimStyle::Spring;
};

class Config {
public:
    static ConfigData Load();
    static void SaveFolderPath(const std::wstring& path);
};
