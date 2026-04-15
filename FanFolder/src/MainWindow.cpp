// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "MainWindow.h"
#include "FanWindow.h"
#include "FileService.h"
#include "Config.h"
#include "Localization.h"
#include "../resources/resource.h"

// DWM constants not always present
#ifndef DWMWA_FORCE_ICONIC_REPRESENTATION
#define DWMWA_FORCE_ICONIC_REPRESENTATION 7
#endif
#ifndef DWMWA_HAS_ICONIC_BITMAP
#define DWMWA_HAS_ICONIC_BITMAP 10
#endif
#ifndef DWMWA_DISALLOW_PEEK
#define DWMWA_DISALLOW_PEEK 11
#endif
#ifndef DWMWA_EXCLUDED_FROM_PEEK
#define DWMWA_EXCLUDED_FROM_PEEK 12
#endif

static constexpr UINT WM_MAIN_SHOW_MIN     = WM_USER + 20;
static constexpr UINT WM_MAIN_PREWARM      = WM_USER + 3;
static constexpr UINT WM_MAIN_CLOSE_FAN    = WM_USER + 4;
static constexpr UINT WM_TRAYICON          = WM_USER + 5;   // tray icon messages
static constexpr int  TOGGLE_COOLDOWN_MS   = 250;

MainWindow* MainWindow::s_instance = nullptr;

// ---------------------------------------------------------------------------
void MainWindow::Register(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = ClassName();
    RegisterClassExW(&wc);
}

MainWindow::MainWindow(HINSTANCE hInst, const ConfigData& config)
    : _hInst(hInst), _config(config)
{
    s_instance = this;
}

MainWindow::~MainWindow() {
    RemoveTrayIcon();
    UninstallHooks();
    if (_hookThread.joinable()) {
        if (_hookThreadId)
            PostThreadMessageW(_hookThreadId, WM_QUIT, 0, 0);
        _hookThread.join();
    }
    s_instance = nullptr;
}

bool MainWindow::Create() {
    _hwnd = CreateWindowExW(
        0,
        ClassName(), L"FanFolder",
        WS_OVERLAPPEDWINDOW | WS_MINIMIZE,
        -32000, -32000, 1, 1,
        nullptr, nullptr, _hInst, this);

    if (!_hwnd) return false;

    // Make it a taskbar button by NOT using WS_EX_TOOLWINDOW
    // Show minimised in taskbar
    ShowWindow(_hwnd, SW_SHOWMINNOACTIVE);

    // DWM iconic thumbnail — show app icon on taskbar hover
    BOOL val = TRUE;
    DwmSetWindowAttribute(_hwnd, DWMWA_FORCE_ICONIC_REPRESENTATION, &val, sizeof(val));
    DwmSetWindowAttribute(_hwnd, DWMWA_HAS_ICONIC_BITMAP,           &val, sizeof(val));
    DwmSetWindowAttribute(_hwnd, DWMWA_DISALLOW_PEEK,               &val, sizeof(val));
    DwmSetWindowAttribute(_hwnd, DWMWA_EXCLUDED_FROM_PEEK,          &val, sizeof(val));

    // Set taskbar icon from embedded resource
    _icoSmall     = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP),
                                       IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    _icoBig       = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP),
                                       IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    _icoOpenSmall = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP_OPEN),
                                       IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    _icoOpenBig   = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP_OPEN),
                                       IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    SetTaskbarIcon(false);

    StartPrewarm();
    AddTrayIcon();
    return true;
}

// ---------------------------------------------------------------------------
void MainWindow::ToggleFan() {
    DWORD now = GetTickCount();
    if (now - _lastToggleTick < TOGGLE_COOLDOWN_MS) return;
    _lastToggleTick = now;

    if (_fanOpen)
        CloseFan();
    else
        OpenFan();
}

void MainWindow::OpenFan() {
    CloseFan();

    // Take ownership of pre-warmed data (items + icons) under the lock
    PrewarmData prewarm;
    {
        std::lock_guard<std::mutex> lk(_prewarmMutex);
        if (_prewarm.ready) {
            prewarm = std::move(_prewarm);
            _prewarm.ready = false;  // reset: std::move leaves bool unchanged in moved-from object
        }
    }

    std::vector<FileItem> items = prewarm.ready
        ? std::move(prewarm.items)
        : !_cachedItems.empty()
            ? _cachedItems   // open instantly with last known list; icons load async
            : FileService::ScanFolder(_config.folderPath, _config.maxItems,
                                      _config.includeDirs, _config.filterRegex, _config.sortMode,
                                      false, _hwnd);

    _fanWindow = std::make_unique<FanWindow>(_hInst, _hwnd, _config, std::move(items));

    // Inject pre-warmed icons; FanWindow::Show() will skip async loading for them
    if (prewarm.ready)
        _fanWindow->AcceptPrewarmIcons(std::move(prewarm.bitmaps),
                                       std::move(prewarm.icons),
                                       prewarm.iconSize);

    if (_fanWindow->Create()) {
        _fanWindow->Show();
        _fanOpen     = true;
        _fanOpenTick = GetTickCount();
        SetTaskbarIcon(true);
        InstallHooks();
    } else {
        _fanWindow.reset();
    }
}

void MainWindow::CloseFan() {
    UninstallHooks();
    if (_fanWindow) {
        _fanWindow->Close();
        _fanWindow.reset();
    }
    _fanOpen = false;
    _lastToggleTick = GetTickCount();  // prevent SC_RESTORE from immediately reopening
    SetTaskbarIcon(false);
    ShowWindow(_hwnd, SW_SHOWMINNOACTIVE);
    StartPrewarm();  // pre-load icons while idle, ready for next open
}

// ---------------------------------------------------------------------------
void MainWindow::SetTaskbarIcon(bool open) {
    HICON sm = open ? _icoOpenSmall : _icoSmall;
    HICON lg = open ? _icoOpenBig   : _icoBig;
    if (sm) SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)sm);
    if (lg) SendMessageW(_hwnd, WM_SETICON, ICON_BIG,   (LPARAM)lg);
}

// ---------------------------------------------------------------------------
void MainWindow::StartPrewarm() {
    ConfigData cfg   = _config;
    HWND       hwnd  = _hwnd;
    int        myGen = ++_prewarmGen;  // invalidates any still-running prewarm thread

    struct PrewarmWork {
        MainWindow* self;
        ConfigData  cfg;
        HWND        hwnd;
        int         myGen;
    };
    auto* work = new PrewarmWork{this, std::move(cfg), hwnd, myGen};

    TrySubmitThreadpoolCallback([](PTP_CALLBACK_INSTANCE, PVOID ctx) {
        auto* pw = static_cast<PrewarmWork*>(ctx);
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        // Calculate icon size using the same formula as FanWindow::CalculateLayout
        int iconSize = 64;
        HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (hTray) {
            RECT tbRect = {};
            GetWindowRect(hTray, &tbRect);
            HMONITOR hMon = MonitorFromRect(&tbRect, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfoW(hMon, &mi);
            int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;
            iconSize = std::clamp(screenH / 19, 48, 128);
        }

        auto items = FileService::ScanFolder(pw->cfg.folderPath, pw->cfg.maxItems,
                                             pw->cfg.includeDirs, pw->cfg.filterRegex, pw->cfg.sortMode);
        auto* data      = new MainWindow::PrewarmData;
        data->items     = items;
        data->iconSize  = iconSize;
        data->ready     = true;
        data->bitmaps.resize(items.size(), nullptr);
        data->icons.resize(items.size(), nullptr);

        for (int i = 0; i < (int)items.size(); i++) {
            const std::wstring& p  = items[i].fullPath;
            const std::wstring& tp = items[i].targetPath.empty() ? p : items[i].targetPath;

            // Online items (SharePoint/OneDrive): p is a URL — use extension-based icon only
            const bool isOnline = (p.size() > 8 &&
                                   (_wcsnicmp(p.c_str(), L"https://", 8) == 0 ||
                                    _wcsnicmp(p.c_str(), L"http://",  7) == 0));

            HBITMAP bmp = nullptr;
            if (!isOnline) {
                if (FileService::IsSvgExtension(tp))
                    bmp = FileService::GetSvgThumbnail(tp, iconSize);
                if (!bmp && FileService::IsGdiImageExtension(tp))
                    bmp = FileService::GetImageThumbnail(tp, iconSize);
                if (!bmp && FileService::IsShellThumbnailExtension(tp))
                    bmp = FileService::GetShellThumbnail(tp, iconSize);
                if (!bmp)
                    bmp = FileService::GetShellBitmap(p, iconSize);
            }

            if (bmp) {
                data->bitmaps[i] = bmp;
            } else if (isOnline) {
                // Use filename (e.g. "document.docx") for file-type icon — not the URL
                const std::wstring& iconSrc = (!tp.empty() && tp.find(L"://") == std::wstring::npos)
                                            ? tp : p;
                data->bitmaps[i] = FileService::GetShellBitmapByExtension(iconSrc, iconSize);
            } else {
                data->icons[i] = FileService::GetShellIcon(p);
            }
        }

        // Only post if still the current generation; discard stale results
        if (pw->myGen != pw->self->_prewarmGen.load()) {
            data->FreeHandles();
            delete data;
            CoUninitialize();
            delete pw;
            return;
        }
        data->gen = pw->myGen;
        PostMessageW(pw->hwnd, WM_MAIN_PREWARM, 0, (LPARAM)data);
        CoUninitialize();
        delete pw;
    }, work, nullptr);
}

// ─── Tray icon ──────────────────────────────────────────────────────────────

void MainWindow::AddTrayIcon() {
    _nid             = {};
    _nid.cbSize      = sizeof(_nid);
    _nid.hWnd        = _hwnd;
    _nid.uID         = 1;
    _nid.uFlags      = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    _nid.uCallbackMessage = WM_TRAYICON;
    _nid.hIcon       = _icoSmall;
    wcscpy_s(_nid.szTip, L"FanFolder");

    Shell_NotifyIconW(NIM_ADD, &_nid);

    // Request modern balloon-capable version
    _nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &_nid);
}

void MainWindow::RemoveTrayIcon() {
    if (_nid.cbSize) {
        Shell_NotifyIconW(NIM_DELETE, &_nid);
        if (_nid.hIcon) { DestroyIcon(_nid.hIcon); _nid.hIcon = nullptr; }
        _nid = {};
    }
}

void MainWindow::ShowTrayMenu() {
    // Menu IDs (mirror FanWindow::ShowSettingsMenu)
    enum {
        ID_SORT_DATE_DESC = 1001,
        ID_SORT_DATE_ASC, ID_SORT_CREATED_DESC, ID_SORT_CREATED_ASC,
        ID_SORT_NAME_ASC, ID_SORT_NAME_DESC,
        ID_MAX_5, ID_MAX_10, ID_MAX_15, ID_MAX_20, ID_MAX_25,
        ID_ANIM_FAN, ID_ANIM_GLIDE, ID_ANIM_SPRING, ID_ANIM_NONE, ID_ANIM_FADE,
        ID_INCLUDE_DIRS,
        ID_SHOW_EXTENSIONS,
        ID_FOLDER_DOWNLOADS,
        ID_FOLDER_DESKTOP,
        ID_FOLDER_DOCUMENTS,
        ID_FOLDER_RECENTDOCS,
        ID_FOLDER_RECENTFILES,
        ID_FOLDER_GRAPHRECENT,
        ID_FOLDER_BROWSE,
        ID_OPEN_FOLDER,
        ID_EXIT,
    };

    const Strings& s = GetStrings();

    HMENU hSort = CreatePopupMenu();
    const bool isRecentMode = (_config.folderPath == L"::RecentDocs::" ||
                                _config.folderPath == L"::GraphRecent::");
    AppendMenuW(hSort, MF_STRING | (_config.sortMode == ConfigData::SortMode::DateModifiedDesc ? MF_CHECKED : 0), ID_SORT_DATE_DESC,    s.sortDateModDesc);
    AppendMenuW(hSort, MF_STRING | (_config.sortMode == ConfigData::SortMode::DateModifiedAsc  ? MF_CHECKED : 0), ID_SORT_DATE_ASC,     s.sortDateModAsc);
    if (!isRecentMode) {
        AppendMenuW(hSort, MF_STRING | (_config.sortMode == ConfigData::SortMode::DateCreatedDesc ? MF_CHECKED : 0), ID_SORT_CREATED_DESC, s.sortDateCreatedDesc);
        AppendMenuW(hSort, MF_STRING | (_config.sortMode == ConfigData::SortMode::DateCreatedAsc  ? MF_CHECKED : 0), ID_SORT_CREATED_ASC,  s.sortDateCreatedAsc);
    }
    AppendMenuW(hSort, MF_STRING | (_config.sortMode == ConfigData::SortMode::NameAsc  ? MF_CHECKED : 0), ID_SORT_NAME_ASC,  s.sortNameAsc);
    AppendMenuW(hSort, MF_STRING | (_config.sortMode == ConfigData::SortMode::NameDesc ? MF_CHECKED : 0), ID_SORT_NAME_DESC, s.sortNameDesc);

    HMENU hMax = CreatePopupMenu();
    for (int n : {5, 10, 15, 20, 25}) {
        UINT id = (UINT)(ID_MAX_5 + (n / 5 - 1));
        AppendMenuW(hMax, MF_STRING | (_config.maxItems == n ? MF_CHECKED : 0), id,
                    std::to_wstring(n).c_str());
    }

    // Resolve well-known folder paths for folder submenu
    auto getKnownPath = [](const KNOWNFOLDERID& id) -> std::wstring {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) {
            std::wstring r(p);
            CoTaskMemFree(p);
            return r;
        }
        return {};
    };
    auto pathEq = [](const std::wstring& a, const std::wstring& b) -> bool {
        return !a.empty() && !b.empty() && _wcsicmp(a.c_str(), b.c_str()) == 0;
    };
    std::wstring dlPath  = getKnownPath(FOLDERID_Downloads);
    std::wstring dtPath  = getKnownPath(FOLDERID_Desktop);
    std::wstring docPath = getKnownPath(FOLDERID_Documents);

    HMENU hFolder = CreatePopupMenu();
    AppendMenuW(hFolder, MF_STRING | (pathEq(_config.folderPath, dtPath)  ? MF_CHECKED : 0), ID_FOLDER_DESKTOP,    s.folderDesktop);
    AppendMenuW(hFolder, MF_STRING | (pathEq(_config.folderPath, docPath) ? MF_CHECKED : 0), ID_FOLDER_DOCUMENTS,  s.folderDocuments);
    AppendMenuW(hFolder, MF_STRING | (pathEq(_config.folderPath, dlPath)  ? MF_CHECKED : 0), ID_FOLDER_DOWNLOADS,  s.folderDownloads);
    AppendMenuW(hFolder, MF_STRING | (_config.folderPath == L"::RecentDocs::"   ? MF_CHECKED : 0), ID_FOLDER_RECENTDOCS,  s.folderRecentDocs);
    AppendMenuW(hFolder, MF_STRING | (_config.folderPath == L"::GraphRecent::" ? MF_CHECKED : 0), ID_FOLDER_GRAPHRECENT, s.folderGraphRecent);
    AppendMenuW(hFolder, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFolder, MF_STRING, ID_FOLDER_BROWSE, s.folderBrowse);

    // Build folder path label (truncated)
    std::wstring folderLabel = s.openPrefix;
    if (_config.folderPath == L"::RecentDocs::") {
        folderLabel += s.folderRecentDocs;
    } else if (_config.folderPath == L"::GraphRecent::") {
        folderLabel += s.folderGraphRecent;
    } else if (_config.folderPath.size() > 40) {
        folderLabel += L"\u2026" + _config.folderPath.substr(_config.folderPath.size() - 38);
    } else {
        folderLabel += _config.folderPath;
    }

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, folderLabel.c_str());
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSort,   s.sortBy);
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hMax,    s.maxItems);
    AppendMenuW(hMenu, MF_STRING | (_config.includeDirs    ? MF_CHECKED : 0), ID_INCLUDE_DIRS,    s.includeFolders);
    AppendMenuW(hMenu, MF_STRING | (_config.showExtensions ? MF_CHECKED : 0), ID_SHOW_EXTENSIONS, s.showExtensions);
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hFolder, s.folderSubmenu);
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_EXIT, s.exitApp);

    // Position menu at cursor; SetForegroundWindow required for proper dismissal
    POINT pt = {};
    GetCursorPos(&pt);
    SetForegroundWindow(_hwnd);
    int cmd = (int)TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                                    pt.x, pt.y, _hwnd, nullptr);
    DestroyMenu(hMenu);

    if (!cmd) return;

    bool changed = true;
    switch (cmd) {
    case ID_SORT_DATE_DESC:    _config.sortMode  = ConfigData::SortMode::DateModifiedDesc; break;
    case ID_SORT_DATE_ASC:     _config.sortMode  = ConfigData::SortMode::DateModifiedAsc;  break;
    case ID_SORT_CREATED_DESC: _config.sortMode  = ConfigData::SortMode::DateCreatedDesc;  break;
    case ID_SORT_CREATED_ASC:  _config.sortMode  = ConfigData::SortMode::DateCreatedAsc;   break;
    case ID_SORT_NAME_ASC:     _config.sortMode  = ConfigData::SortMode::NameAsc;          break;
    case ID_SORT_NAME_DESC:    _config.sortMode  = ConfigData::SortMode::NameDesc;         break;
    case ID_MAX_5:          _config.maxItems  = 5;  break;
    case ID_MAX_10:         _config.maxItems  = 10; break;
    case ID_MAX_15:         _config.maxItems  = 15; break;
    case ID_MAX_20:         _config.maxItems  = 20; break;
    case ID_MAX_25:         _config.maxItems  = 25; break;
    case ID_ANIM_FAN:       _config.animStyle = ConfigData::AnimStyle::Fan;    break;
    case ID_ANIM_GLIDE:     _config.animStyle = ConfigData::AnimStyle::Glide;  break;
    case ID_ANIM_SPRING:    _config.animStyle = ConfigData::AnimStyle::Spring; break;
    case ID_ANIM_FADE:      _config.animStyle = ConfigData::AnimStyle::Fade;   break;
    case ID_ANIM_NONE:      _config.animStyle = ConfigData::AnimStyle::None;   break;
    case ID_INCLUDE_DIRS:   _config.includeDirs    = !_config.includeDirs;    break;
    case ID_SHOW_EXTENSIONS:_config.showExtensions = !_config.showExtensions; break;
    case ID_FOLDER_DOWNLOADS: {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &p))) {
            _config.folderPath = p; CoTaskMemFree(p);
        }
        break;
    }
    case ID_FOLDER_DESKTOP: {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &p))) {
            _config.folderPath = p; CoTaskMemFree(p);
        }
        break;
    }
    case ID_FOLDER_DOCUMENTS: {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p))) {
            _config.folderPath = p; CoTaskMemFree(p);
        }
        break;
    }
    case ID_FOLDER_RECENTDOCS:
        _config.folderPath = L"::RecentDocs::";
        break;
    case ID_FOLDER_GRAPHRECENT:
        _config.folderPath = L"::GraphRecent::";
        break;
    case ID_FOLDER_BROWSE: {
        IFileOpenDialog* pfd = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&pfd)))) {
            DWORD opts = 0;
            pfd->GetOptions(&opts);
            pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
            pfd->SetTitle(GetStrings().selectFolderDlg);
            if (!_config.folderPath.empty()) {
                IShellItem* psi = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(_config.folderPath.c_str(),
                                                          nullptr, IID_PPV_ARGS(&psi)))) {
                    pfd->SetFolder(psi);
                    psi->Release();
                }
            }
            if (SUCCEEDED(pfd->Show(_hwnd))) {
                IShellItem* psi = nullptr;
                if (SUCCEEDED(pfd->GetResult(&psi))) {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                        _config.folderPath = path;
                        CoTaskMemFree(path);
                    }
                    psi->Release();
                }
            }
            pfd->Release();
        }
        break;
    }
    case ID_EXIT:
        changed = false;
        PostMessageW(_hwnd, WM_CLOSE, 0, 0);
        return;
    default:
        changed = false;
        break;
    }

    if (changed) {
        Config::Save(_config);
        // Keep old prewarm alive — it will be replaced when the new one completes.
        // This ensures the fan still shows something if opened before the new prewarm finishes.
        PostMessageW(_hwnd, WM_MAIN_SHOW_MIN, 0, 0); // ensure minimized
        StartPrewarm();
    }
}

// ---------------------------------------------------------------------------
void MainWindow::InstallHooks() {
    if (_hookThread.joinable()) return;

    HWND hwnd = _hwnd;
    _hookThread = std::thread([this, hwnd]() {
        _hookThreadId = GetCurrentThreadId();

        _mouseHook  = SetWindowsHookExW(WH_MOUSE_LL,    MouseHookProc,    nullptr, 0);
        _kbHook     = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, nullptr, 0);
        _hWinEvent  = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                      nullptr, WinEventProc, 0, 0,
                                      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (_mouseHook) { UnhookWindowsHookEx(_mouseHook); _mouseHook = nullptr; }
        if (_kbHook)    { UnhookWindowsHookEx(_kbHook);    _kbHook    = nullptr; }
        if (_hWinEvent) { UnhookWinEvent(_hWinEvent);       _hWinEvent = nullptr; }
    });
}

void MainWindow::UninstallHooks() {
    if (_hookThread.joinable()) {
        if (_hookThreadId)
            PostThreadMessageW(_hookThreadId, WM_QUIT, 0, 0);
        _hookThread.join();
        _hookThreadId = 0;
    }
}

// ---------------------------------------------------------------------------
LRESULT CALLBACK MainWindow::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance) {
        bool isDown = (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
                       wParam == WM_MBUTTONDOWN  || wParam == WM_NCLBUTTONDOWN);
        if (isDown && s_instance->_fanWindow) {
            HWND fanHwnd = s_instance->_fanWindow->Handle();
            if (fanHwnd) {
                // ms->pt is in virtual-screen (physical) pixels — NOT the same coordinate
                // space as GetWindowRect for a PerMonitorV2-aware process.
                // GetCursorPos always matches GetWindowRect: both use the process's
                // logical DPI coordinate space.
                POINT pt = {};
                GetCursorPos(&pt);
                RECT rc = {};
                GetWindowRect(fanHwnd, &rc);
                if (pt.x < rc.left || pt.x >= rc.right ||
                    pt.y < rc.top  || pt.y >= rc.bottom) {
                    PostMessageW(s_instance->_hwnd, WM_MAIN_CLOSE_FAN, 0, 0);
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK MainWindow::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (kb->vkCode == VK_ESCAPE)
            PostMessageW(s_instance->_hwnd, WM_MAIN_CLOSE_FAN, 0, 0);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void CALLBACK MainWindow::WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd,
                                        LONG idObject, LONG, DWORD idEventThread, DWORD) {
    if (event != EVENT_SYSTEM_FOREGROUND || !s_instance || !s_instance->_fanOpen)
        return;
    if (idObject != OBJID_WINDOW || hwnd == nullptr)
        return;

    // Ignore foreground changes that happen right after opening (e.g. taskbar click)
    DWORD now = GetTickCount();
    if (now - s_instance->_fanOpenTick < 500)
        return;

    // Close the fan if the newly foregrounded window belongs to a different process
    DWORD fgPid = 0;
    GetWindowThreadProcessId(hwnd, &fgPid);
    if (fgPid != GetCurrentProcessId()) {
        // Don't interrupt an active shell drag — the drop target will foreground naturally
        if (s_instance->_fanWindow && s_instance->_fanWindow->IsDragging())
            return;
        PostMessageW(s_instance->_hwnd, WM_MAIN_CLOSE_FAN, 0, 0);
    }
}

// ---------------------------------------------------------------------------
void MainWindow::ProvideIconicThumbnail(int w, int h) {
    if (w <= 0 || h <= 0) return;

    static constexpr int SrcSize = 256;
    HICON hIco = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP),
                                    IMAGE_ICON, SrcSize, SrcSize, LR_DEFAULTCOLOR);
    if (!hIco)
        hIco = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP),
                                  IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
    if (!hIco) return;

    // Draw into a 256×256 DIB — DrawIconEx preserves premultiplied alpha
    BITMAPINFOHEADER bh = {};
    bh.biSize = sizeof(bh); bh.biWidth = SrcSize; bh.biHeight = -SrcSize;
    bh.biPlanes = 1; bh.biBitCount = 32; bh.biCompression = BI_RGB;
    void* srcBits = nullptr;
    HBITMAP hSrc = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bh),
                                     DIB_RGB_COLORS, &srcBits, nullptr, 0);
    if (!hSrc) { DestroyIcon(hIco); return; }
    {
        HDC hdc = CreateCompatibleDC(nullptr);
        auto* hOld = SelectObject(hdc, hSrc);
        ZeroMemory(srcBits, SrcSize * SrcSize * 4);
        DrawIconEx(hdc, 0, 0, hIco, SrcSize, SrcSize, 0, nullptr, DI_NORMAL);
        GdiFlush();
        SelectObject(hdc, hOld);
        DeleteDC(hdc);
    }
    DestroyIcon(hIco);

    // Scale to requested thumbnail size with high-quality bicubic
    int iconSize = std::min(w, h);
    Gdiplus::Bitmap srcGdi(SrcSize, SrcSize, SrcSize * 4,
                           PixelFormat32bppPARGB, static_cast<BYTE*>(srcBits));
    Gdiplus::Bitmap dstGdi(w, h, PixelFormat32bppPARGB);
    {
        Gdiplus::Graphics g(&dstGdi);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.DrawImage(&srcGdi, (w - iconSize) / 2, (h - iconSize) / 2, iconSize, iconSize);
    }
    DeleteObject(hSrc);

    HBITMAP hOut = nullptr;
    dstGdi.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hOut);
    if (hOut) { DwmSetIconicThumbnail(_hwnd, hOut, 0); DeleteObject(hOut); }
}

// ---------------------------------------------------------------------------
MainWindow* MainWindow::FromHWND(HWND hwnd) {
    return reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    MainWindow* self = FromHWND(hwnd);
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_RESTORE) {
            self->ToggleFan();
            DefWindowProcW(hwnd, msg, wParam, lParam);
            PostMessageW(hwnd, WM_MAIN_SHOW_MIN, 0, 0);
            return 0;
        }
        break;

    case WM_MAIN_SHOW_MIN:
        ShowWindow(hwnd, SW_SHOWMINNOACTIVE);
        return 0;

    case WM_DWMSENDICONICTHUMBNAIL:
        self->ProvideIconicThumbnail(HIWORD(lParam), LOWORD(lParam));
        return 0;

    case WM_ACTIVATEAPP:
        if (wParam == 0 && self->_fanOpen) {
            // Don't close while a drag is in progress — OLE drag activates the drop target
            if (self->_fanWindow && self->_fanWindow->IsDragging())
                return 0;
            DWORD now = GetTickCount();
            if (now - self->_fanOpenTick > 500)
                self->CloseFan();
        }
        return 0;

    case WM_ACTIVATE:
        if (wParam != WA_INACTIVE)
            ShowWindow(hwnd, SW_MINIMIZE);
        return 0;

    case WM_MAIN_PREWARM: {
        auto* data = reinterpret_cast<PrewarmData*>(lParam);
        if (data) {
            std::lock_guard<std::mutex> lk(self->_prewarmMutex);
            // Discard if a newer prewarm was started after this one was posted
            if (data->gen == self->_prewarmGen.load()) {
                self->_prewarm.FreeHandles();
                self->_prewarm = std::move(*data);
                // Keep a copy of the item list as fallback for fast open
                self->_cachedItems = self->_prewarm.items;
            } else {
                data->FreeHandles();
            }
            delete data;
        }
        return 0;
    }

    case FanWindow::WM_SETTINGS_CHANGED: {
        auto* cfg = reinterpret_cast<ConfigData*>(lParam);
        if (cfg) {
            self->_config = *cfg;
            delete cfg;
        }
        // Keep old prewarm alive — CloseFan → StartPrewarm will replace it with fresh data.
        return 0;
    }

    case WM_MAIN_CLOSE_FAN: // WM_USER+4
        self->CloseFan();
        return 0;

    case WM_TRAYICON:
        // lParam is the mouse/interaction event when using NOTIFYICON_VERSION_4
        switch (LOWORD(lParam)) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            self->ShowTrayMenu();
            return 0;
        case WM_LBUTTONUP:
        case NIN_SELECT:
            self->ToggleFan();
            return 0;
        }
        return 0;

    case WM_DESTROY:
        self->CloseFan();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
