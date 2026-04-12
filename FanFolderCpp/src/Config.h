#pragma once
#include "pch.h"

struct ConfigData {
    std::wstring folderPath;
    int  maxItems       = 15;
    bool includeDirs    = true;
    bool showExtensions = false;
    std::wstring filterRegex;
    enum class SortMode { DateModifiedDesc, DateModifiedAsc, NameAsc, NameDesc }
        sortMode = SortMode::DateModifiedDesc;
    enum class AnimStyle { Fan, Glide, Spring, None, Fade }
        animStyle = AnimStyle::Spring;
};

class Config {
public:
    static ConfigData Load();
    static void Save(const ConfigData& cfg);         // save all fields at once
    static void SaveFolderPath(const std::wstring& path);
    static void SaveSortMode(ConfigData::SortMode mode);
    static void SaveMaxItems(int count);
    static void SaveIncludeDirs(bool include);
    static void SaveFilterRegex(const std::wstring& pattern);
    static void SaveAnimStyle(ConfigData::AnimStyle style);
    static void SaveShowExtensions(bool show);
};
