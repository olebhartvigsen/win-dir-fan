#include "pch.h"
#include "MainWindow.h"
#include "FanWindow.h"
#include "FileService.h"
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

static constexpr UINT WM_MAIN_SHOW_MIN  = WM_USER + 20;
static constexpr UINT WM_MAIN_PREWARM   = WM_USER + 3;
static constexpr UINT WM_MAIN_CLOSE_FAN = WM_USER + 4;
static constexpr int  TOGGLE_COOLDOWN_MS = 250;

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
        ClassName(), L"Fan Folder",
        WS_OVERLAPPEDWINDOW | WS_MINIMIZE,
        -32000, -32000, 1, 1,
        nullptr, nullptr, _hInst, this);

    if (!_hwnd) return false;

    // Make it a taskbar button by NOT using WS_EX_TOOLWINDOW
    // Show minimised in taskbar
    ShowWindow(_hwnd, SW_SHOWMINNOACTIVE);

    // DWM iconic thumbnail attributes
    BOOL val = TRUE;
    DwmSetWindowAttribute(_hwnd, DWMWA_FORCE_ICONIC_REPRESENTATION, &val, sizeof(val));
    DwmSetWindowAttribute(_hwnd, DWMWA_HAS_ICONIC_BITMAP, &val, sizeof(val));
    DwmSetWindowAttribute(_hwnd, DWMWA_DISALLOW_PEEK, &val, sizeof(val));
    DwmSetWindowAttribute(_hwnd, DWMWA_EXCLUDED_FROM_PEEK, &val, sizeof(val));

    // Set taskbar icon from embedded resource
    HICON hIcoSmall = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP),
                                         IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    HICON hIcoBig   = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP),
                                         IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (hIcoSmall) SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcoSmall);
    if (hIcoBig)   SendMessageW(_hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIcoBig);

    StartPrewarm();
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
        if (_prewarm.ready)
            prewarm = std::move(_prewarm);
    }

    std::vector<FileItem> items = prewarm.ready
        ? prewarm.items
        : FileService::ScanFolder(_config.folderPath, _config.maxItems,
                                  _config.includeDirs, _config.filterRegex, _config.sortMode);

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
    ShowWindow(_hwnd, SW_SHOWMINNOACTIVE);
    StartPrewarm();  // pre-load icons while idle, ready for next open
}

// ---------------------------------------------------------------------------
void MainWindow::StartPrewarm() {
    ConfigData cfg  = _config;
    HWND       hwnd = _hwnd;
    std::thread([this, cfg, hwnd]() {
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

        auto items = FileService::ScanFolder(cfg.folderPath, cfg.maxItems,
                                             cfg.includeDirs, cfg.filterRegex, cfg.sortMode);
        auto* data      = new PrewarmData;
        data->items     = items;
        data->iconSize  = iconSize;
        data->ready     = true;
        data->bitmaps.resize(items.size(), nullptr);
        data->icons.resize(items.size(), nullptr);

        for (int i = 0; i < (int)items.size(); i++) {
            const std::wstring& p = items[i].fullPath;
            HBITMAP bmp = nullptr;
            if (FileService::IsGdiImageExtension(p))
                bmp = FileService::GetImageThumbnail(p, iconSize);
            if (!bmp && FileService::IsShellThumbnailExtension(p))
                bmp = FileService::GetShellThumbnail(p, iconSize);
            if (!bmp)
                bmp = FileService::GetShellBitmap(p, iconSize);
            if (bmp) {
                data->bitmaps[i] = bmp;
            } else {
                data->icons[i] = FileService::GetShellIcon(p);
            }
        }

        PostMessageW(hwnd, WM_MAIN_PREWARM, 0, (LPARAM)data);
    }).detach();
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
    if (fgPid != GetCurrentProcessId())
        PostMessageW(s_instance->_hwnd, WM_MAIN_CLOSE_FAN, 0, 0);
}

// ---------------------------------------------------------------------------
void MainWindow::ProvideIconicThumbnail(int w, int h) {
    if (w <= 0 || h <= 0) return;

    // Load at 256×256 — the largest standard icon size, gives crisp source
    static constexpr int SrcSize = 256;
    HICON hIco = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP),
                                    IMAGE_ICON, SrcSize, SrcSize, LR_DEFAULTCOLOR);
    if (!hIco)
        hIco = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP),
                                  IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
    if (!hIco) return;

    // Step 1: draw the icon into a 256×256 DIB so DrawIconEx preserves alpha
    auto makeDib = [](int sz, void** ppBits) -> HBITMAP {
        BITMAPINFOHEADER bh = {};
        bh.biSize = sizeof(bh); bh.biWidth = sz; bh.biHeight = -sz;
        bh.biPlanes = 1; bh.biBitCount = 32; bh.biCompression = BI_RGB;
        return CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&bh),
                                DIB_RGB_COLORS, ppBits, nullptr, 0);
    };

    void* srcBits = nullptr;
    HBITMAP hSrc = makeDib(SrcSize, &srcBits);
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

    // Step 2: scale to thumbnail size using GDI+ high-quality bicubic
    int iconSize = std::min(w, h);
    int x = (w - iconSize) / 2;
    int y = (h - iconSize) / 2;

    // Wrap source DIB bits in a GDI+ Bitmap (premultiplied ARGB — matches DrawIconEx output)
    Gdiplus::Bitmap srcGdi(SrcSize, SrcSize, SrcSize * 4, PixelFormat32bppPARGB,
                           static_cast<BYTE*>(srcBits));

    Gdiplus::Bitmap dstGdi(w, h, PixelFormat32bppPARGB);
    {
        Gdiplus::Graphics g(&dstGdi);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.DrawImage(&srcGdi, x, y, iconSize, iconSize);
    }
    DeleteObject(hSrc);

    // Step 3: get HBITMAP for DwmSetIconicThumbnail
    HBITMAP hOut = nullptr;
    dstGdi.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hOut);
    if (hOut) {
        DwmSetIconicThumbnail(_hwnd, hOut, 0);
        DeleteObject(hOut);
    }
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
            self->_prewarm.FreeHandles();
            self->_prewarm = std::move(*data);
            delete data;
        }
        return 0;
    }

    case WM_MAIN_CLOSE_FAN: // WM_USER+4
        self->CloseFan();
        return 0;

    case WM_DESTROY:
        self->CloseFan();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
