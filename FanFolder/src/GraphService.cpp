// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "GraphService.h"
#include <winhttp.h>
#include <wincred.h>
#include <nlohmann/json.hpp>

// C++/WinRT — WAM (Windows Account Manager) silent auth
#define WINRT_LEAN_AND_MEAN
#include <inspectable.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Security.Authentication.Web.Core.h>
#include <winrt/Windows.Security.Credentials.h>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr char    kClientId[]   = "66d15612-bee9-4126-ba51-a811a31bfd15";
static constexpr char    kScope[]      = "Files.Read User.Read offline_access";
static constexpr wchar_t kCredTarget[] = L"FanFolder/Graph/RefreshToken";

static constexpr wchar_t kAuthHost[]   = L"login.microsoftonline.com";
static constexpr wchar_t kGraphHost[]  = L"graph.microsoft.com";

// /me/drive/recent — recently accessed files, requires only Files.Read (no admin consent)
static constexpr wchar_t kRecentPath[] =
    L"/v1.0/me/drive/recent"
    L"?$top=50"
    L"&$select=name,webUrl,fileSystemInfo,file,remoteItem";

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------
static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

// URL-decode a UTF-8 string (simple %XX decoder)
static std::wstring UrlDecodeToWide(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            char hex[3] = { s[i+1], s[i+2], 0 };
            out += (char)strtol(hex, nullptr, 16);
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return Utf8ToWide(out);
}

// Extract filename (with extension) from a URL or path
static std::wstring FilenameFromUri(const std::string& uri) {
    // Strip query/fragment
    std::string work = uri;
    auto q = work.find('?'); if (q != std::string::npos) work.resize(q);
    auto f = work.find('#'); if (f != std::string::npos) work.resize(f);
    auto sep = work.rfind('/');
    if (sep == std::string::npos) return Utf8ToWide(work);
    return UrlDecodeToWide(work.substr(sep + 1));
}

// Parse ISO-8601 datetime string to FILETIME (UTC)
static FILETIME ParseIso8601(const std::string& dt) {
    SYSTEMTIME st = {};
    // Format: "2026-04-13T12:34:56Z" or "2026-04-13T12:34:56.000Z"
    sscanf_s(dt.c_str(), "%hd-%hd-%hdT%hd:%hd:%hd",
             &st.wYear, &st.wMonth, &st.wDay,
             &st.wHour, &st.wMinute, &st.wSecond);
    FILETIME ft = {};
    SystemTimeToFileTime(&st, &ft);
    return ft;
}

// Copy text to clipboard
static void CopyToClipboard(HWND hwnd, const std::wstring& text) {
    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        memcpy(GlobalLock(hMem), text.c_str(), bytes);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
}

// ---------------------------------------------------------------------------
// WinHTTP helpers — all HTTPS, no auth
// ---------------------------------------------------------------------------
static std::string HttpPost(const wchar_t* host, const wchar_t* path,
                             const std::string& body)
{
    HINTERNET hSess = WinHttpOpen(L"FanFolder/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return {};

    std::string result;
    HINTERNET hConn = WinHttpConnect(hSess, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConn) {
        HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", path,
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (hReq) {
            WinHttpAddRequestHeaders(hReq,
                L"Content-Type: application/x-www-form-urlencoded",
                (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   (LPVOID)body.c_str(), (DWORD)body.size(),
                                   (DWORD)body.size(), 0)
                && WinHttpReceiveResponse(hReq, nullptr))
            {
                DWORD avail = 0;
                while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
                    std::string chunk(avail, '\0');
                    DWORD read = 0;
                    WinHttpReadData(hReq, chunk.data(), avail, &read);
                    chunk.resize(read);
                    result += chunk;
                }
            }
            WinHttpCloseHandle(hReq);
        }
        WinHttpCloseHandle(hConn);
    }
    WinHttpCloseHandle(hSess);
    return result;
}

static std::string HttpGet(const wchar_t* host, const wchar_t* path,
                            const std::string& bearerToken)
{
    HINTERNET hSess = WinHttpOpen(L"FanFolder/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return {};

    std::string result;
    HINTERNET hConn = WinHttpConnect(hSess, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConn) {
        HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", path,
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (hReq) {
            std::wstring auth = L"Authorization: Bearer " + Utf8ToWide(bearerToken);
            WinHttpAddRequestHeaders(hReq, auth.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            WinHttpAddRequestHeaders(hReq,
                L"Accept: application/json", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   nullptr, 0, 0, 0)
                && WinHttpReceiveResponse(hReq, nullptr))
            {
                DWORD avail = 0;
                while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
                    std::string chunk(avail, '\0');
                    DWORD read = 0;
                    WinHttpReadData(hReq, chunk.data(), avail, &read);
                    chunk.resize(read);
                    result += chunk;
                }
            }
            WinHttpCloseHandle(hReq);
        }
        WinHttpCloseHandle(hConn);
    }
    WinHttpCloseHandle(hSess);
    return result;
}

// ---------------------------------------------------------------------------
// Credential Manager — store / load refresh token
// ---------------------------------------------------------------------------
static void StoreRefreshToken(const std::wstring& token) {
    CREDENTIALW cred = {};
    cred.Type              = CRED_TYPE_GENERIC;
    cred.TargetName        = const_cast<wchar_t*>(kCredTarget);
    cred.CredentialBlobSize = (DWORD)(token.size() * sizeof(wchar_t));
    cred.CredentialBlob    = (LPBYTE)token.c_str();
    cred.Persist           = CRED_PERSIST_LOCAL_MACHINE;
    CredWriteW(&cred, 0);
}

static std::wstring LoadRefreshToken() {
    PCREDENTIALW pCred = nullptr;
    if (!CredReadW(kCredTarget, CRED_TYPE_GENERIC, 0, &pCred)) return {};
    std::wstring token(
        reinterpret_cast<wchar_t*>(pCred->CredentialBlob),
        pCred->CredentialBlobSize / sizeof(wchar_t));
    CredFree(pCred);
    return token;
}

// ---------------------------------------------------------------------------
// Token operations
// ---------------------------------------------------------------------------

// Exchange a refresh token for a new access token. Returns "" on failure.
static std::string RefreshAccessToken(const std::wstring& refreshToken) {
    std::string body =
        "client_id=" + std::string(kClientId) +
        "&grant_type=refresh_token"
        "&scope=" + std::string(kScope) +
        "&refresh_token=" + WideToUtf8(refreshToken);

    auto resp = HttpPost(kAuthHost, L"/common/oauth2/v2.0/token", body);
    if (resp.empty()) return {};

    auto j = json::parse(resp, nullptr, false);
    if (j.is_discarded() || !j.contains("access_token")) return {};

    // Update stored refresh token if a new one was issued
    if (j.contains("refresh_token")) {
        StoreRefreshToken(Utf8ToWide(j["refresh_token"].get<std::string>()));
    }
    return j["access_token"].get<std::string>();
}

// ---------------------------------------------------------------------------
// Device code auth — shows a dialog, opens browser, polls for completion
// ---------------------------------------------------------------------------

struct DeviceAuthCtx {
    std::string  deviceCode;
    int          interval    = 5;
    std::string  accessToken;
    std::wstring refreshToken;
    std::wstring errorMsg;
    std::atomic<int> status{ 0 }; // 0=pending, 1=done, 2=error
    HWND         hwndDialog  = nullptr;
};

static DWORD WINAPI PollThread(LPVOID param) {
    auto* ctx = static_cast<DeviceAuthCtx*>(param);
    std::string body =
        "client_id=" + std::string(kClientId) +
        "&grant_type=urn:ietf:params:oauth:grant-type:device_code"
        "&device_code=" + ctx->deviceCode;

    while (ctx->status.load() == 0) {
        Sleep(ctx->interval * 1000);
        if (ctx->status.load() != 0) break;

        auto resp = HttpPost(kAuthHost, L"/common/oauth2/v2.0/token", body);
        if (resp.empty()) continue;

        auto j = json::parse(resp, nullptr, false);
        if (j.is_discarded()) continue;

        if (j.contains("access_token")) {
            ctx->accessToken  = j["access_token"].get<std::string>();
            ctx->refreshToken = j.contains("refresh_token")
                ? Utf8ToWide(j["refresh_token"].get<std::string>()) : L"";
            ctx->status.store(1); // done
            if (ctx->hwndDialog)
                PostMessage(ctx->hwndDialog, WM_COMMAND,
                            MAKEWPARAM(IDOK, BN_CLICKED), 0);
        } else {
            std::string err = j.value("error", "");
            if (err != "authorization_pending" && err != "slow_down") {
                ctx->errorMsg = Utf8ToWide(j.value("error_description", err));
                ctx->status.store(2); // error
                if (ctx->hwndDialog)
                    PostMessage(ctx->hwndDialog, WM_COMMAND,
                                MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Custom auth dialog — pure USER32, no COMCTL32 v6 / TaskDialogIndirect needed
// ---------------------------------------------------------------------------
static constexpr wchar_t kAuthWndClass[] = L"FanFolderAuthDlg";
static constexpr UINT    WM_AUTH_DONE    = WM_APP + 101;

struct AuthDlgData {
    DeviceAuthCtx* ctx;
    std::wstring   code;
    std::wstring   verifyUrl;
};

static LRESULT CALLBACK AuthWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    AuthDlgData* d = reinterpret_cast<AuthDlgData*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        d = reinterpret_cast<AuthDlgData*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));

        HINSTANCE hInst = cs->hInstance;
        int margin = 16, W = 416;

        // Instruction line
        CreateWindowExW(0, L"STATIC",
            L"Sign in to access your recent Microsoft 365 files.",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            margin, 14, W - 2*margin, 18, hwnd, (HMENU)100, hInst, nullptr);

        // Code + URL edit (read-only)
        std::wstring msg2 =
            L"Go to: " + d->verifyUrl +
            L"\r\nEnter code:  " + d->code +
            L"\r\n\r\n(Code copied to clipboard — browser opened automatically)";
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", msg2.c_str(),
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY
            | WS_VSCROLL | ES_AUTOVSCROLL,
            margin, 40, W - 2*margin, 80, hwnd, (HMENU)101, hInst, nullptr);

        // Status label
        CreateWindowExW(0, L"STATIC", L"Waiting for sign-in\u2026",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            margin, 134, W - 2*margin - 90, 18, hwnd, (HMENU)102, hInst, nullptr);

        // Cancel button
        CreateWindowExW(0, L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
            W - margin - 80, 128, 80, 26, hwnd, (HMENU)IDCANCEL, hInst, nullptr);

        SetTimer(hwnd, 1, 500, nullptr);
        return 0;
    }
    case WM_TIMER: {
        if (!d) return 0;
        int st = d->ctx->status.load();
        if (st == 1) {
            SetDlgItemTextW(hwnd, 102, L"Sign-in successful!");
            PostMessageW(hwnd, WM_AUTH_DONE, 0, 0);
        } else if (st == 2) {
            PostMessageW(hwnd, WM_AUTH_DONE, 0, 0);
        }
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDCANCEL) {
            if (d) d->ctx->status.store(2);
            PostMessageW(hwnd, WM_AUTH_DONE, 0, 0);
        } else if (LOWORD(wp) == IDOK) {
            PostMessageW(hwnd, WM_AUTH_DONE, 0, 0);
        }
        return 0;
    case WM_AUTH_DONE:
        KillTimer(hwnd, 1);
        DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        if (d) d->ctx->status.store(2);
        PostMessageW(hwnd, WM_AUTH_DONE, 0, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterAuthWndClass() {
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc   = { sizeof(wc) };
    wc.lpfnWndProc   = AuthWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = kAuthWndClass;
    RegisterClassExW(&wc);
    done = true;
}

// Performs device code auth and returns access token on success.
static std::string DoDeviceCodeAuth(HWND hwnd) {
    // 1. Request device code
    std::string reqBody =
        "client_id=" + std::string(kClientId) +
        "&scope=" + std::string(kScope);
    auto resp = HttpPost(kAuthHost, L"/common/oauth2/v2.0/devicecode", reqBody);
    if (resp.empty()) return {};

    auto j = json::parse(resp, nullptr, false);
    if (j.is_discarded() || !j.contains("device_code")) return {};

    DeviceAuthCtx ctx;
    ctx.deviceCode = j["device_code"].get<std::string>();
    ctx.interval   = j.value("interval", 5);

    std::string userCode   = j.value("user_code",        "");
    std::string verifyUri  = j.value("verification_uri", "https://microsoft.com/devicelogin");

    std::wstring wCode    = Utf8ToWide(userCode);
    std::wstring wVerify  = Utf8ToWide(verifyUri);

    // 2. Copy user code to clipboard and open browser
    CopyToClipboard(hwnd, wCode);
    ShellExecuteA(hwnd, "open", verifyUri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    // 3. Show custom auth dialog (modal, pure USER32)
    RegisterAuthWndClass();

    int dlgW = 448, dlgH = 172;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    int dx = (sx - dlgW) / 2, dy = (sy - dlgH) / 2;
    if (hwnd) {
        RECT rc; GetWindowRect(hwnd, &rc);
        dx = rc.left + (rc.right - rc.left - dlgW) / 2;
        dy = rc.top  + (rc.bottom - rc.top - dlgH) / 2;
    }

    AuthDlgData dlgData{ &ctx, wCode, wVerify };
    HWND hdlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        kAuthWndClass,
        L"Fan Folder \u2014 Sign in to Microsoft 365",
        WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        dx, dy, dlgW, dlgH,
        hwnd, nullptr, GetModuleHandleW(nullptr), &dlgData);

    if (!hdlg) return {};
    ctx.hwndDialog = hdlg;

    // Start background polling thread
    CloseHandle(CreateThread(nullptr, 0, PollThread, &ctx, 0, nullptr));

    // Modal message loop — exits when hdlg is destroyed
    if (hwnd) EnableWindow(hwnd, FALSE);
    MSG msg2;
    while (IsWindow(hdlg) && GetMessageW(&msg2, nullptr, 0, 0)) {
        TranslateMessage(&msg2);
        DispatchMessageW(&msg2);
    }
    if (hwnd) EnableWindow(hwnd, TRUE);

    if (ctx.status.load() == 1 && !ctx.accessToken.empty()) {
        if (!ctx.refreshToken.empty())
            StoreRefreshToken(ctx.refreshToken);
        return ctx.accessToken;
    }
    return {};
}

// ---------------------------------------------------------------------------
// IWebAuthenticationCoreManagerInterop — Win32 HWND-aware WAM interop
// Stable COM interface; defined here to avoid requiring the interop header.
// ---------------------------------------------------------------------------
MIDL_INTERFACE("F4B8E804-811E-4436-B69C-44CB67B72084")
IWebAuthenticationCoreManagerInterop : public IInspectable
{
    virtual HRESULT STDMETHODCALLTYPE RequestTokenForWindowAsync(
        HWND appWindow, IInspectable* request,
        REFIID riid, void** asyncOperation) = 0;
    virtual HRESULT STDMETHODCALLTYPE RequestTokenWithWebAccountForWindowAsync(
        HWND appWindow, IInspectable* request, IInspectable* webAccount,
        REFIID riid, void** asyncOperation) = 0;
};

// ---------------------------------------------------------------------------
// WAM (Windows Account Manager) — silent auth using existing Windows/Entra ID session.
// On domain-joined machines the PRT covers Graph access without consent prompts.
// Returns the access token, or "" if unavailable.
// ---------------------------------------------------------------------------
static std::string TryWamAuth(HWND hwnd) {
    std::string result;
    HANDLE hDone = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    std::thread([&result, hwnd, hDone] {
        try {
            winrt::init_apartment(winrt::apartment_type::single_threaded);

            namespace WAC = winrt::Windows::Security::Authentication::Web::Core;

            // Try both common and organizations authority
            winrt::Windows::Security::Credentials::WebAccountProvider provider{ nullptr };
            for (auto authority : { L"organizations", L"common" }) {
                provider = WAC::WebAuthenticationCoreManager::FindAccountProviderAsync(
                    L"https://login.microsoft.com", authority).get();
                if (provider) break;
            }

            if (!provider) {
                winrt::uninit_apartment();
                SetEvent(hDone);
                return;
            }

            // .default scope uses whatever the PRT covers (best for domain-joined machines)
            WAC::WebTokenRequest request(
                provider,
                L"https://graph.microsoft.com/.default",
                L"66d15612-bee9-4126-ba51-a811a31bfd15");

            // Silent first — uses PRT, no UI at all
            auto silentResult = WAC::WebAuthenticationCoreManager::GetTokenSilentlyAsync(request).get();

            if (silentResult.ResponseStatus() == WAC::WebTokenRequestStatus::Success) {
                result = winrt::to_string(silentResult.ResponseData().GetAt(0).Token());
            } else if (silentResult.ResponseStatus() == WAC::WebTokenRequestStatus::UserInteractionRequired && hwnd) {
                // Interactive via Win32 interop (parents dialog to our window, not browser)
                try {
                    auto interop = winrt::get_activation_factory<WAC::WebAuthenticationCoreManager,
                                                                  IWebAuthenticationCoreManagerInterop>();
                    winrt::Windows::Foundation::IAsyncOperation<WAC::WebTokenRequestResult> op{ nullptr };
                    auto rawReq = reinterpret_cast<IInspectable*>(winrt::get_abi(request));
                    HRESULT hr = interop->RequestTokenForWindowAsync(
                        hwnd, rawReq,
                        winrt::guid_of<winrt::Windows::Foundation::IAsyncOperation<WAC::WebTokenRequestResult>>(),
                        winrt::put_abi(op));
                    if (SUCCEEDED(hr) && op) {
                        auto interResult = op.get();
                        if (interResult.ResponseStatus() == WAC::WebTokenRequestStatus::Success)
                            result = winrt::to_string(interResult.ResponseData().GetAt(0).Token());
                    }
                } catch (...) {}
            }

            winrt::uninit_apartment();
        } catch (...) {}
        SetEvent(hDone);
    }).detach();

    WaitForSingleObject(hDone, 30000);
    CloseHandle(hDone);
    return result;
}

// ---------------------------------------------------------------------------
// GetAccessToken — WAM first (domain PRT), then refresh token, then device code
// ---------------------------------------------------------------------------
static std::string GetAccessToken(HWND hwnd) {
    // 1. Try WAM — uses existing Windows/Entra ID domain session (PRT), no consent screen
    auto wamToken = TryWamAuth(hwnd);
    if (!wamToken.empty()) return wamToken;

    // 2. Try stored refresh token (from a previous successful sign-in)
    auto stored = LoadRefreshToken();
    if (!stored.empty()) {
        auto token = RefreshAccessToken(stored);
        if (!token.empty()) return token;
    }

    // 3. WAM and silent auth both failed.
    // Do NOT automatically open the browser — tenant admin consent may be required.
    // Show a clear message instead.
    MessageBoxW(hwnd,
        L"Sign-in failed.\n\n"
        L"Your organisation requires administrator approval for third-party apps.\n\n"
        L"Ask your IT administrator to approve 'FanFolder' by visiting:\n"
        L"https://login.microsoftonline.com/common/adminconsent"
        L"?client_id=66d15612-bee9-4126-ba51-a811a31bfd15\n\n"
        L"Alternatively, use the 'Seneste' mode which reads your recent files without sign-in.",
        L"Fan Folder \u2014 Sign-in required",
        MB_OK | MB_ICONINFORMATION);
    return {};
}

// ---------------------------------------------------------------------------
// Parse Graph /me/drive/recent response into FileItems
// ---------------------------------------------------------------------------
static std::vector<FileItem> ParseDriveRecentResponse(const std::string& body,
                                                       int maxItems)
{
    std::vector<FileItem> out;
    auto j = json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.contains("value")) return out;

    for (const auto& item : j["value"]) {
        if ((int)out.size() >= maxItems) break;

        // Skip folders
        if (!item.contains("file") && !item.contains("remoteItem")) continue;
        if (item.contains("folder")) continue;

        // Name and web URL
        std::string name   = item.value("name", "");
        std::string webUrl = item.value("webUrl", "");

        // remoteItem has its own webUrl (SharePoint/Teams files)
        if (item.contains("remoteItem")) {
            const auto& ri = item["remoteItem"];
            if (webUrl.empty()) webUrl = ri.value("webUrl", "");
            if (name.empty())   name   = ri.value("name",   "");
            // remoteItem may also contain fileSystemInfo
            if (item["remoteItem"].contains("fileSystemInfo")) {
                // prefer remoteItem timestamps below
            }
        }

        if (webUrl.empty() && name.empty()) continue;

        // Timestamps from fileSystemInfo
        FILETIME lastAccessed = {}, lastModified = {};
        auto extractTimes = [&](const json& fsi) {
            std::string la = fsi.value("lastAccessedDateTime", "");
            std::string lm = fsi.value("lastModifiedDateTime", "");
            if (!la.empty()) lastAccessed = ParseIso8601(la);
            if (!lm.empty()) lastModified  = ParseIso8601(lm);
        };
        if (item.contains("fileSystemInfo"))
            extractTimes(item["fileSystemInfo"]);
        if (item.contains("remoteItem") && item["remoteItem"].contains("fileSystemInfo"))
            extractTimes(item["remoteItem"]["fileSystemInfo"]);

        FILETIME sortTime = (lastAccessed.dwHighDateTime || lastAccessed.dwLowDateTime)
                          ? lastAccessed : lastModified;
        if (!sortTime.dwHighDateTime && !sortTime.dwLowDateTime) continue;

        std::wstring wName = Utf8ToWide(name);
        std::wstring wUrl  = webUrl.empty() ? L"" : Utf8ToWide(webUrl);

        FileItem fi;
        fi.fullPath      = wUrl.empty() ? wName : wUrl;
        fi.name          = wName;
        fi.targetPath    = wName;   // used for icon resolution by extension
        fi.isDirectory   = false;
        fi.lastWriteTime = sortTime;
        fi.creationTime  = (lastModified.dwHighDateTime || lastModified.dwLowDateTime)
                         ? lastModified : sortTime;

        out.push_back(std::move(fi));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
std::vector<FileItem> GraphService::GetRecentItems(HWND hwnd, int maxItems) {
    std::string token = GetAccessToken(hwnd);
    if (token.empty()) return {};

    auto body = HttpGet(kGraphHost, kRecentPath, token);
    if (body.empty()) return {};

    // If token was rejected (401), clear stored token and re-auth once
    if (body.find("\"code\":\"InvalidAuthenticationToken\"") != std::string::npos
        || body.find("\"code\":\"Unauthorized\"") != std::string::npos)
    {
        GraphService::SignOut();
        token = GetAccessToken(hwnd);
        if (token.empty()) return {};
        body = HttpGet(kGraphHost, kRecentPath, token);
    }

    return ParseDriveRecentResponse(body, maxItems);
}

void GraphService::SignOut() {
    CredDeleteW(kCredTarget, CRED_TYPE_GENERIC, 0);
}
