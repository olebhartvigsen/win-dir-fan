// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "Config.h"

ConfigData Config::Load() {
    ConfigData cfg;

    // 1. Registry: HKCU\SOFTWARE\FanFolder
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\FanFolder", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            // FolderPath
            {
                wchar_t buf[MAX_PATH] = {};
                DWORD size = sizeof(buf);
                DWORD type = REG_SZ;
                if (RegQueryValueExW(hKey, L"FolderPath", nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
                    std::wstring path(buf);
                    if (!path.empty() && (path == L"::RecentDocs::" || path == L"::RecentFiles::" || path == L"::GraphRecent::" || GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES))
                        cfg.folderPath = path;
                }
            }
            // SortMode
            {
                wchar_t buf[64] = {};
                DWORD size = sizeof(buf);
                DWORD type = REG_SZ;
                if (RegQueryValueExW(hKey, L"SortMode", nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
                    if (wcscmp(buf, L"DateModifiedAsc") == 0)  cfg.sortMode = ConfigData::SortMode::DateModifiedAsc;
                    else if (wcscmp(buf, L"NameAsc") == 0)     cfg.sortMode = ConfigData::SortMode::NameAsc;
                    else if (wcscmp(buf, L"NameDesc") == 0)    cfg.sortMode = ConfigData::SortMode::NameDesc;
                    else if (wcscmp(buf, L"DateCreatedDesc") == 0) cfg.sortMode = ConfigData::SortMode::DateCreatedDesc;
                    else if (wcscmp(buf, L"DateCreatedAsc") == 0)  cfg.sortMode = ConfigData::SortMode::DateCreatedAsc;
                }
            }
            // MaxItems (try DWORD first, then SZ)
            {
                DWORD val = 0;
                DWORD size = sizeof(val);
                DWORD type = 0;
                if (RegQueryValueExW(hKey, L"MaxItems", nullptr, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS && type == REG_DWORD) {
                    int n = (int)val;
                    if (n >= 1 && n <= 50) cfg.maxItems = n;
                } else {
                    wchar_t buf[16] = {};
                    DWORD sz2 = sizeof(buf);
                    DWORD t2 = REG_SZ;
                    if (RegQueryValueExW(hKey, L"MaxItems", nullptr, &t2, (LPBYTE)buf, &sz2) == ERROR_SUCCESS) {
                        int n = _wtoi(buf);
                        if (n >= 1 && n <= 50) cfg.maxItems = n;
                    }
                }
            }
            // IncludeDirectories
            {
                DWORD val = 1;
                DWORD size = sizeof(val);
                DWORD type = REG_DWORD;
                if (RegQueryValueExW(hKey, L"IncludeDirectories", nullptr, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS && type == REG_DWORD) {
                    cfg.includeDirs = (val != 0);
                }
            }
            // FilterRegex
            {
                wchar_t buf[512] = {};
                DWORD size = sizeof(buf);
                DWORD type = REG_SZ;
                if (RegQueryValueExW(hKey, L"FilterRegex", nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
                    cfg.filterRegex = buf;
                }
            }
            // ShowExtensions
            {
                DWORD val = 0;
                DWORD size = sizeof(val);
                DWORD type = REG_DWORD;
                if (RegQueryValueExW(hKey, L"ShowExtensions", nullptr, &type, (LPBYTE)&val, &size) == ERROR_SUCCESS && type == REG_DWORD) {
                    cfg.showExtensions = (val != 0);
                }
            }
            // AnimationStyle
            {
                wchar_t buf[64] = {};
                DWORD size = sizeof(buf);
                DWORD type = REG_SZ;
                if (RegQueryValueExW(hKey, L"AnimationStyle", nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
                    if (wcscmp(buf, L"Fan") == 0)         cfg.animStyle = ConfigData::AnimStyle::Fan;
                    else if (wcscmp(buf, L"Glide") == 0)  cfg.animStyle = ConfigData::AnimStyle::Glide;
                    else if (wcscmp(buf, L"None") == 0)   cfg.animStyle = ConfigData::AnimStyle::None;
                    else if (wcscmp(buf, L"Fade") == 0)   cfg.animStyle = ConfigData::AnimStyle::Fade;
                    else                                   cfg.animStyle = ConfigData::AnimStyle::Glide;
                }
            }
            RegCloseKey(hKey);
        }
    }

    if (!cfg.folderPath.empty())
        return cfg;

    // 2. appsettings.json next to exe
    {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring dir(exePath);
        auto pos = dir.rfind(L'\\');
        if (pos != std::wstring::npos)
            dir = dir.substr(0, pos);
        std::wstring jsonPath = dir + L"\\appsettings.json";

        std::ifstream f(jsonPath);
        if (f.is_open()) {
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            const std::string key = "\"FanFolderPath\"";
            auto kpos = content.find(key);
            if (kpos != std::string::npos) {
                auto colon = content.find(':', kpos + key.size());
                if (colon != std::string::npos) {
                    auto q1 = content.find('"', colon + 1);
                    if (q1 != std::string::npos) {
                        auto q2 = content.find('"', q1 + 1);
                        if (q2 != std::string::npos) {
                            std::string val = content.substr(q1 + 1, q2 - q1 - 1);
                            int len = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, nullptr, 0);
                            if (len > 1) {
                                std::wstring wval(len - 1, L'\0');
                                MultiByteToWideChar(CP_UTF8, 0, val.c_str(), -1, wval.data(), len);
                                if (!wval.empty() && GetFileAttributesW(wval.c_str()) != INVALID_FILE_ATTRIBUTES) {
                                    cfg.folderPath = wval;
                                    return cfg;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 3. Default: Seneste filer (::RecentDocs::) — no path validation needed
    cfg.folderPath = L"::RecentDocs::";
    return cfg;
}

void Config::Save(const ConfigData& cfg) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\FanFolder", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return;

    // FolderPath
    RegSetValueExW(hKey, L"FolderPath", 0, REG_SZ,
                   (const BYTE*)cfg.folderPath.c_str(),
                   (DWORD)((cfg.folderPath.size() + 1) * sizeof(wchar_t)));

    // SortMode
    const wchar_t* sortStr = L"DateModifiedDesc";
    switch (cfg.sortMode) {
    case ConfigData::SortMode::DateModifiedAsc: sortStr = L"DateModifiedAsc"; break;
    case ConfigData::SortMode::NameAsc:         sortStr = L"NameAsc";         break;
    case ConfigData::SortMode::NameDesc:        sortStr = L"NameDesc";        break;
    case ConfigData::SortMode::DateCreatedDesc: sortStr = L"DateCreatedDesc"; break;
    case ConfigData::SortMode::DateCreatedAsc:  sortStr = L"DateCreatedAsc";  break;
    default: break;
    }
    RegSetValueExW(hKey, L"SortMode", 0, REG_SZ,
                   (const BYTE*)sortStr, (DWORD)((wcslen(sortStr) + 1) * sizeof(wchar_t)));

    // MaxItems
    DWORD maxItems = (DWORD)cfg.maxItems;
    RegSetValueExW(hKey, L"MaxItems", 0, REG_DWORD, (const BYTE*)&maxItems, sizeof(maxItems));

    // IncludeDirectories
    DWORD includeDirs = cfg.includeDirs ? 1 : 0;
    RegSetValueExW(hKey, L"IncludeDirectories", 0, REG_DWORD, (const BYTE*)&includeDirs, sizeof(includeDirs));

    // ShowExtensions
    DWORD showExt = cfg.showExtensions ? 1 : 0;
    RegSetValueExW(hKey, L"ShowExtensions", 0, REG_DWORD, (const BYTE*)&showExt, sizeof(showExt));

    // FilterRegex
    RegSetValueExW(hKey, L"FilterRegex", 0, REG_SZ,
                   (const BYTE*)cfg.filterRegex.c_str(),
                   (DWORD)((cfg.filterRegex.size() + 1) * sizeof(wchar_t)));

    // AnimationStyle
    const wchar_t* animStr = L"Glide";
    switch (cfg.animStyle) {
    case ConfigData::AnimStyle::Fan:    animStr = L"Fan";    break;
    case ConfigData::AnimStyle::Spring: animStr = L"Spring"; break;
    case ConfigData::AnimStyle::None:   animStr = L"None";   break;
    case ConfigData::AnimStyle::Fade:   animStr = L"Fade";   break;
    default: break;
    }
    RegSetValueExW(hKey, L"AnimationStyle", 0, REG_SZ,
                   (const BYTE*)animStr, (DWORD)((wcslen(animStr) + 1) * sizeof(wchar_t)));

    RegCloseKey(hKey);
}
