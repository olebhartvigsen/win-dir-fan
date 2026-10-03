// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "Telemetry.h"

#include <winhttp.h>
#include <winver.h>
#include <bcrypt.h>
#include <random>
#include <sstream>
#include <iomanip>

namespace {
    constexpr wchar_t kRegistryPath[] = L"SOFTWARE\\FanFolder";
    constexpr wchar_t kTelemetrySentValue[] = L"TelemetryFirstRunSent";
    constexpr wchar_t kTelemetryInstallationIdValue[] = L"TelemetryInstallationId";
    constexpr wchar_t kTelemetryHost[] = L"eu.aptabase.com";
    constexpr wchar_t kTelemetryPath[] = L"/api/v0/events";
    constexpr wchar_t kAptabaseAppKey[] = L"A-EU-8526773383";
    // Must stay in lockstep with FILEVERSION/VALUE "FileVersion" in
    // FanFolder/resources/app.rc. Used only when the version resource cannot
    // be read at runtime, so a stale value reports the wrong app version.
    constexpr char kFallbackAppVersion[] = "1.4.3";

#if defined(_DEBUG)
    constexpr bool kIsDebugBuild = true;
#else
    constexpr bool kIsDebugBuild = false;
#endif

    std::string EscapeJsonString(const std::string& value) {
        std::ostringstream escaped;
        for (unsigned char c : value) {
            switch (c) {
            case '\"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (c < 0x20) {
                    escaped << "\\u" << std::hex << std::setw(4)
                            << std::setfill('0') << static_cast<unsigned int>(c);
                } else {
                    escaped << static_cast<char>(c);
                }
                break;
            }
        }
        return escaped.str();
    }

    std::string GenerateId() {
        // A real CSPRNG. std::random_device returns a single 32-bit value on
        // MSVC, so seeding mt19937_64 from it left the whole 128-bit ID
        // predictable from 32 bits of entropy (and colliding across installs).
        // BCryptGenRandom is the platform CSPRNG and needs no extra link: it
        // lives in bcrypt.dll, which loads with the OS.
        unsigned char bytes[16];
        NTSTATUS st = BCryptGenRandom(nullptr, bytes, sizeof(bytes),
                                      BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (st != 0) {
            // Fall back to whatever entropy we can get rather than sending a
            // predictable identifier.
            std::random_device device;
            for (size_t i = 0; i < sizeof(bytes); ++i)
                bytes[i] = static_cast<unsigned char>(device());
        }
        std::ostringstream id;
        id << std::hex << std::setfill('0');
        for (unsigned char b : bytes)
            id << std::setw(2) << static_cast<unsigned>(b);
        return id.str();
    }

    std::wstring ToWide(const std::string& value) {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                             nullptr, 0);
        if (size <= 0) return {};
        std::wstring result(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                            result.data(), size);
        return result;
    }

    std::string ToUtf8(const wchar_t* value) {
        if (!value || !value[0]) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1) return {};
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
        result.resize(size - 1);
        return result;
    }

    std::string GetArchitecture() {
#if defined(_M_ARM64)
        return "arm64";
#elif defined(_M_X64)
        return "x64";
#elif defined(_M_IX86)
        return "x86";
#else
        return "unknown";
#endif
    }

    std::string GetTimestamp() {
        SYSTEMTIME utc = {};
        GetSystemTime(&utc);
        std::ostringstream timestamp;
        timestamp << std::setfill('0')
                  << std::setw(4) << utc.wYear << '-'
                  << std::setw(2) << utc.wMonth << '-'
                  << std::setw(2) << utc.wDay << 'T'
                  << std::setw(2) << utc.wHour << ':'
                  << std::setw(2) << utc.wMinute << ':'
                  << std::setw(2) << utc.wSecond << 'Z';
        return timestamp.str();
    }

    std::string GetAppVersion() {
        wchar_t modulePath[MAX_PATH] = {};
        if (!GetModuleFileNameW(nullptr, modulePath, _countof(modulePath)))
            return kFallbackAppVersion;

        DWORD ignored = 0;
        const DWORD infoSize = GetFileVersionInfoSizeW(modulePath, &ignored);
        if (!infoSize) return kFallbackAppVersion;

        std::vector<BYTE> versionData(infoSize);
        if (!GetFileVersionInfoW(modulePath, 0, infoSize, versionData.data()))
            return kFallbackAppVersion;

        VS_FIXEDFILEINFO* fixedInfo = nullptr;
        UINT fixedInfoSize = 0;
        if (!VerQueryValueW(versionData.data(), L"\\", reinterpret_cast<LPVOID*>(&fixedInfo), &fixedInfoSize) ||
            !fixedInfo || fixedInfoSize < sizeof(VS_FIXEDFILEINFO))
            return kFallbackAppVersion;

        std::ostringstream version;
        version << HIWORD(fixedInfo->dwProductVersionMS) << '.'
                << LOWORD(fixedInfo->dwProductVersionMS) << '.'
                << HIWORD(fixedInfo->dwProductVersionLS);
        return version.str();
    }

    bool IsTelemetryEnabled() {
        HKEY key = nullptr;
        const LONG openResult =
            RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_QUERY_VALUE, &key);

        if (openResult != ERROR_SUCCESS) {
            // Distinguish the two failure modes. Telemetry is opt-OUT, so a key
            // that simply does not exist yet means the user has not opted out:
            // that is the normal state on a fresh install, so it stays enabled.
            // Only a key we are forbidden to read (ACCESS_DENIED and friends)
            // fails closed, because then we cannot know what the user chose.
            return openResult == ERROR_FILE_NOT_FOUND;
        }

        DWORD value = 1;
        DWORD valueSize = sizeof(value);
        DWORD valueType = 0;
        const LONG result = RegQueryValueExW(key, L"TelemetryEnabled", nullptr, &valueType,
                                             reinterpret_cast<LPBYTE>(&value), &valueSize);
        RegCloseKey(key);
        return result != ERROR_SUCCESS || valueType != REG_DWORD || value != 0;
    }

    bool OpenTelemetryKey(HKEY* key, REGSAM access) {
        return RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr,
                               REG_OPTION_NON_VOLATILE, access, nullptr, key, nullptr) == ERROR_SUCCESS;
    }

    // Set when we discover we cannot honour the opt-out or persist the "sent"
    // flag. Once set, no further reporting happens for the rest of the session.
    std::atomic<bool> g_telemetryDisabled{ false };

    bool IsFirstRunAlreadySent() {
        HKEY key = nullptr;
        const LONG openResult =
            RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_QUERY_VALUE, &key);

        if (openResult != ERROR_SUCCESS) {
            // A missing key means the flag was never written, so this really is
            // a first run. But if the key exists and we cannot read it, we
            // cannot tell whether the event already went out, and re-sending it
            // on every launch is worse than skipping one report.
            return openResult != ERROR_FILE_NOT_FOUND;
        }

        DWORD value = 0;
        DWORD valueSize = sizeof(value);
        DWORD valueType = 0;
        const LONG result = RegQueryValueExW(key, kTelemetrySentValue, nullptr, &valueType,
                                             reinterpret_cast<LPBYTE>(&value), &valueSize);
        RegCloseKey(key);
        return result == ERROR_SUCCESS && valueType == REG_DWORD && value != 0;
    }

    std::string GetOrCreateInstallationId() {
        HKEY key = nullptr;
        if (!OpenTelemetryKey(&key, KEY_QUERY_VALUE | KEY_SET_VALUE)) return {};

        wchar_t value[64] = {};
        DWORD valueSize = sizeof(value);
        DWORD valueType = 0;
        const LONG result = RegQueryValueExW(key, kTelemetryInstallationIdValue, nullptr, &valueType,
                                             reinterpret_cast<LPBYTE>(value), &valueSize);
        if (result == ERROR_SUCCESS && valueType == REG_SZ && value[0]) {
            std::string existing = ToUtf8(value);
            RegCloseKey(key);
            return existing;
        }

        std::string generated;
        try {
            generated = GenerateId();
        } catch (...) {
            RegCloseKey(key);
            return {};
        }
        const std::wstring generatedWide = ToWide(generated);
        if (generatedWide.empty() ||
            RegSetValueExW(key, kTelemetryInstallationIdValue, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(generatedWide.c_str()),
                           static_cast<DWORD>((generatedWide.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
            RegCloseKey(key);
            return {};
        }
        RegCloseKey(key);
        return generated;
    }

    void MarkFirstRunSent() {
        HKEY key = nullptr;
        if (!OpenTelemetryKey(&key, KEY_SET_VALUE)) {
            // Cannot persist the "sent" flag, so we would re-send on every
            // launch. Disable for this session rather than repeat forever.
            g_telemetryDisabled.store(true, std::memory_order_release);
            return;
        }

        constexpr DWORD sent = 1;
        const LONG result = RegSetValueExW(key, kTelemetrySentValue, 0, REG_DWORD,
                                          reinterpret_cast<const BYTE*>(&sent), sizeof(sent));
        RegCloseKey(key);
        if (result != ERROR_SUCCESS) {
            // A write-protected key means the flag will never stick, so the same
            // install event would go out on every single launch. Stop here.
            g_telemetryDisabled.store(true, std::memory_order_release);
        }
    }

    void SendFirstRun(const std::string& installationId) {
        const std::string appVersion = GetAppVersion();
        const std::string body =
            "[{\"timestamp\":\"" + GetTimestamp() + "\","
            "\"sessionId\":\"" + EscapeJsonString(installationId) + "\","
            "\"eventName\":\"app_installed\","
            "\"systemProps\":{"
                "\"isDebug\":" + std::string(kIsDebugBuild ? "true" : "false") + ","
                "\"locale\":\"en-US\","
                "\"appVersion\":\"" + EscapeJsonString(appVersion) + "\","
                "\"sdkVersion\":\"winhttp\","
                "\"osName\":\"Windows\"},"
            "\"props\":{"
                "\"architecture\":\"" + GetArchitecture() + "\"}}]";

        HINTERNET session = WinHttpOpen(L"FanFolder",
                                        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return;

        WinHttpSetTimeouts(session, 3000, 3000, 3000, 3000);

        HINTERNET connection = WinHttpConnect(session, kTelemetryHost,
                                              INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connection) {
            WinHttpCloseHandle(session);
            return;
        }

        HINTERNET request = WinHttpOpenRequest(connection, L"POST", kTelemetryPath,
                                               nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               WINHTTP_FLAG_SECURE);
        if (request) {
            const std::wstring headers =
                std::wstring(L"App-Key: ") + kAptabaseAppKey +
                L"\r\nContent-Type: application/json\r\n";
            const BOOL sent = WinHttpSendRequest(request, headers.c_str(),
                                                 static_cast<DWORD>(-1L),
                                                 const_cast<char*>(body.data()),
                                                 static_cast<DWORD>(body.size()),
                                                 static_cast<DWORD>(body.size()), 0);
            if (sent && WinHttpReceiveResponse(request, nullptr)) {
                DWORD status = 0;
                DWORD statusSize = sizeof(status);
                if (WinHttpQueryHeaders(request,
                                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                                        WINHTTP_NO_HEADER_INDEX) && status >= 200 && status < 300) {
                    MarkFirstRunSent();
                }
            }
            WinHttpCloseHandle(request);
        }

        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
    }
}

void Telemetry::ReportFirstRun() {
    if (g_telemetryDisabled.load(std::memory_order_acquire)) return;
    if (!IsTelemetryEnabled()) return;
    if (IsFirstRunAlreadySent()) return;

    const std::string installationId = GetOrCreateInstallationId();
    if (installationId.empty()) return;

    try {
        std::thread([installationId] {
            try {
                SendFirstRun(installationId);
            } catch (...) {
                // Telemetry must never terminate the application.
            }
        }).detach();
    } catch (...) {
        // Telemetry must never prevent FanFolder from starting.
    }
}
