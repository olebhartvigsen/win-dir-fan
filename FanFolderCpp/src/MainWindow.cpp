#include "pch.h"
#include "MainWindow.h"
#include "FanWindow.h"
#include "FileService.h"

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
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
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

    // Set taskbar icon
    HICON hIco = CreateStackIcon(32);
    if (hIco) {
        SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIco);
        SendMessageW(_hwnd, WM_SETICON, ICON_BIG,   (LPARAM)hIco);
    }

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

    std::vector<FileItem> items;
    {
        std::lock_guard<std::mutex> lk(_prewarmMutex);
        if (_prewarmReady)
            items = _prewarmItems;
    }
    if (items.empty())
        items = FileService::ScanFolder(_config.folderPath, _config.maxItems,
                                        _config.includeDirs, _config.filterRegex, _config.sortMode);

    _fanWindow = std::make_unique<FanWindow>(_hInst, _hwnd, _config, std::move(items));
    if (_fanWindow->Create()) {
        _fanWindow->Show();
        _fanOpen     = true;
        _fanOpenTick = GetTickCount();
        InstallHooks();
        StartPrewarm();
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
    ShowWindow(_hwnd, SW_SHOWMINNOACTIVE);
}

// ---------------------------------------------------------------------------
void MainWindow::StartPrewarm() {
    ConfigData cfg = _config;
    HWND hwnd = _hwnd;
    std::thread([this, cfg, hwnd]() {
        auto items = FileService::ScanFolder(cfg.folderPath, cfg.maxItems,
                                             cfg.includeDirs, cfg.filterRegex, cfg.sortMode);
        auto* vec  = new std::vector<FileItem>(std::move(items));
        PostMessageW(hwnd, WM_MAIN_PREWARM, 0, (LPARAM)vec);
    }).detach();
}

// ---------------------------------------------------------------------------
void MainWindow::InstallHooks() {
    if (_hookThread.joinable()) return;

    HWND hwnd = _hwnd;
    _hookThread = std::thread([this, hwnd]() {
        _hookThreadId = GetCurrentThreadId();

        _mouseHook = SetWindowsHookExW(WH_MOUSE_LL,    MouseHookProc,    nullptr, 0);
        _kbHook    = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, nullptr, 0);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (_mouseHook) { UnhookWindowsHookEx(_mouseHook); _mouseHook = nullptr; }
        if (_kbHook)    { UnhookWindowsHookEx(_kbHook);    _kbHook    = nullptr; }
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
            MSLLHOOKSTRUCT* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            HWND fanHwnd = s_instance->_fanWindow->Handle();
            if (fanHwnd) {
                RECT rc;
                GetWindowRect(fanHwnd, &rc);
                POINT pt = ms->pt;
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

// ---------------------------------------------------------------------------
void MainWindow::ProvideIconicThumbnail(int w, int h) {
    HICON hIco = CreateStackIcon(std::min(w, h));
    if (!hIco) return;

    ICONINFO ii = {};
    GetIconInfo(hIco, &ii);
    if (ii.hbmColor) {
        DwmSetIconicThumbnail(_hwnd, ii.hbmColor, 0);
        DeleteObject(ii.hbmColor);
    }
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    DestroyIcon(hIco);
}

// ---------------------------------------------------------------------------
HICON MainWindow::CreateStackIcon(int size) {
    if (size <= 0) size = 64;

    Gdiplus::Bitmap bmp(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&bmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        float s   = (float)size;
        float dw  = s * 0.55f;
        float dh  = s * 0.68f;
        float cx  = s * 0.5f;
        float cy  = s * 0.5f;
        float r   = s * 0.08f;
        float fold = s * 0.15f;

        // Helper lambda to draw a single document sheet
        auto drawDoc = [&](float offX, float offY, float angle,
                           BYTE rr, BYTE gg, BYTE bb) {
            Gdiplus::Matrix mat;
            mat.RotateAt(angle, Gdiplus::PointF(cx, cy));
            g.SetTransform(&mat);

            float left = cx - dw / 2.f + offX;
            float top  = cy - dh / 2.f + offY;

            Gdiplus::GraphicsPath path;
            // Body (all corners except top-right which is folded)
            path.AddLine(left,            top + fold,   left,          top + dh);
            path.AddLine(left,            top + dh,     left + dw,     top + dh);
            path.AddLine(left + dw,       top + dh,     left + dw,     top + fold);
            path.AddLine(left + dw,       top + fold,   left + dw - fold, top);
            path.AddLine(left + dw - fold, top,         left,          top + fold);
            path.CloseFigure();

            Gdiplus::SolidBrush brush(Gdiplus::Color(230, rr, gg, bb));
            g.FillPath(&brush, &path);

            Gdiplus::Pen pen(Gdiplus::Color(180, (BYTE)(rr/2), (BYTE)(gg/2), (BYTE)(bb/2)), 1.f);
            g.DrawPath(&pen, &path);

            // Fold triangle
            Gdiplus::GraphicsPath foldPath;
            foldPath.AddLine(left + dw - fold, top,      left + dw,     top + fold);
            foldPath.AddLine(left + dw,        top + fold, left + dw - fold, top + fold);
            foldPath.CloseFigure();
            Gdiplus::SolidBrush foldBrush(Gdiplus::Color(160, (BYTE)(rr*3/4), (BYTE)(gg*3/4), (BYTE)(bb*3/4)));
            g.FillPath(&foldBrush, &foldPath);

            g.ResetTransform();
        };

        // Blue doc behind left
        drawDoc(-s * 0.08f, s * 0.04f, -22.f, 70, 130, 220);
        // Amber doc behind right
        drawDoc( s * 0.08f, s * 0.04f,  22.f, 230, 160, 40);
        // White doc in front
        drawDoc(0.f, 0.f, 0.f, 240, 240, 245);
    }

    HICON hIcon = nullptr;
    bmp.GetHICON(&hIcon);
    return hIcon;
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

    case WM_MAIN_PREWARM: { // WM_USER+3
        auto* vec = reinterpret_cast<std::vector<FileItem>*>(lParam);
        if (vec) {
            std::lock_guard<std::mutex> lk(self->_prewarmMutex);
            self->_prewarmItems  = std::move(*vec);
            self->_prewarmReady  = true;
            delete vec;
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
