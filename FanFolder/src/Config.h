// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#pragma once
#include "pch.h"

struct ConfigData {
    std::wstring folderPath;
    int  maxItems       = 15;
    bool includeDirs    = true;
    bool showExtensions = false;
    std::wstring filterRegex;
    enum class SortMode { DateModifiedDesc, DateModifiedAsc, NameAsc, NameDesc, DateCreatedDesc, DateCreatedAsc }
        sortMode = SortMode::DateModifiedDesc;
    enum class AnimStyle { Fan, Glide, Spring, None, Fade }
        animStyle = AnimStyle::Glide;
    bool showInfoOnLaunch = false;  // tray toggle: show the info panel on every launch (default off)
    bool infoScreenShown  = false;  // one-time marker: the first-run info panel has been shown
};

class Config {
public:
    static ConfigData Load();
    static void Save(const ConfigData& cfg);         // save all fields at once
};
