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

    // Field-wise equality, used to decide whether a cached prewarm is still
    // valid for the live config.  Defaulted deliberately: a hand-written
    // compare silently ignores any setting added later, which would let a
    // stale scan masquerade as fresh after e.g. a maxItems change.
    bool operator==(const ConfigData&) const = default;
};

class Config {
public:
    static ConfigData Load();
    static void Save(const ConfigData& cfg);         // save all fields at once
};
