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
                    if (!path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
                        cfg.folderPath = path;
                }
            }
            // SortMode
            {
                wchar_t buf[64] = {};
                DWORD size = sizeof(buf);
                DWORD type = REG_SZ;
                if (RegQueryValueExW(hKey, L"SortMode", nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
                    std::wstring s(buf);
                    if (s == L"DateModifiedAsc") cfg.sortMode = ConfigData::SortMode::DateModifiedAsc;
                    else if (s == L"NameAsc")    cfg.sortMode = ConfigData::SortMode::NameAsc;
                    else if (s == L"NameDesc")   cfg.sortMode = ConfigData::SortMode::NameDesc;
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
            // AnimationStyle
            {
                wchar_t buf[64] = {};
                DWORD size = sizeof(buf);
                DWORD type = REG_SZ;
                if (RegQueryValueExW(hKey, L"AnimationStyle", nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
                    std::wstring s(buf);
                    if (s == L"Fan")    cfg.animStyle = ConfigData::AnimStyle::Fan;
                    else if (s == L"Glide") cfg.animStyle = ConfigData::AnimStyle::Glide;
                    else if (s == L"None")  cfg.animStyle = ConfigData::AnimStyle::None;
                    // else stays Spring
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

    // 3. Downloads folder
    {
        wchar_t* pPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &pPath))) {
            std::wstring path(pPath);
            CoTaskMemFree(pPath);
            if (!path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                cfg.folderPath = path;
                return cfg;
            }
        }
    }

    // 4. Desktop folder
    {
        wchar_t buf[MAX_PATH] = {};
        if (SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, buf) == S_OK) {
            cfg.folderPath = buf;
        }
    }

    return cfg;
}

void Config::Save(const ConfigData& cfg) {
    SaveFolderPath(cfg.folderPath);
    SaveSortMode(cfg.sortMode);
    SaveMaxItems(cfg.maxItems);
    SaveIncludeDirs(cfg.includeDirs);
    SaveFilterRegex(cfg.filterRegex);
    SaveAnimStyle(cfg.animStyle);
}

void Config::SaveFolderPath(const std::wstring& path) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\FanFolder", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"FolderPath", 0, REG_SZ,
                       (const BYTE*)path.c_str(),
                       (DWORD)((path.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

void Config::SaveSortMode(ConfigData::SortMode mode) {
    const wchar_t* s = L"DateModifiedDesc";
    switch (mode) {
    case ConfigData::SortMode::DateModifiedAsc: s = L"DateModifiedAsc"; break;
    case ConfigData::SortMode::NameAsc:         s = L"NameAsc";         break;
    case ConfigData::SortMode::NameDesc:        s = L"NameDesc";        break;
    default: break;
    }
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\FanFolder", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"SortMode", 0, REG_SZ,
                       (const BYTE*)s, (DWORD)((wcslen(s) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

void Config::SaveMaxItems(int count) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\FanFolder", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = (DWORD)count;
        RegSetValueExW(hKey, L"MaxItems", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

void Config::SaveIncludeDirs(bool include) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\FanFolder", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        DWORD val = include ? 1 : 0;
        RegSetValueExW(hKey, L"IncludeDirectories", 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
}

void Config::SaveFilterRegex(const std::wstring& pattern) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\FanFolder", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"FilterRegex", 0, REG_SZ,
                       (const BYTE*)pattern.c_str(),
                       (DWORD)((pattern.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

void Config::SaveAnimStyle(ConfigData::AnimStyle style) {
    const wchar_t* s = L"Spring";
    switch (style) {
    case ConfigData::AnimStyle::Fan:   s = L"Fan";   break;
    case ConfigData::AnimStyle::Glide: s = L"Glide"; break;
    case ConfigData::AnimStyle::None:  s = L"None";  break;
    default: break;
    }
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\FanFolder", 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"AnimationStyle", 0, REG_SZ,
                       (const BYTE*)s, (DWORD)((wcslen(s) + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}
