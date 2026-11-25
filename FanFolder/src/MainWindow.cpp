// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "MainWindow.h"
#include "FanWindow.h"
#include "FileService.h"
#include "Config.h"
#include "Localization.h"
#include "InfoScreen.h"
#include "../resources/resource.h"
#include <fstream>

// Debug log helper — writes to %TEMP%\fanfolder_debug.log
// Gated by kDebugLogEnabled: set to true for diagnostic builds. When false,
// this is a no-op so we don't hammer the disk (and spend CPU formatting) on
// every mouse click, window message, and 60 FPS animation tick.
static constexpr bool kDebugLogEnabled = false;
static void DebugLog(const wchar_t* msg) {
    if constexpr (!kDebugLogEnabled) return;
    OutputDebugStringW(msg);
    static wchar_t logPath[MAX_PATH] = {};
    if (!logPath[0]) {
        GetTempPathW(MAX_PATH, logPath);
        wcscat_s(logPath, L"fanfolder_debug.log");
    }
    // FILE_SHARE_READ|WRITE so tail readers can see it; open/close per write for reliability
    HANDLE h = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    // UTF-8 encode for simplicity
    int n = WideCharToMultiByte(CP_UTF8, 0, msg, -1, nullptr, 0, nullptr, nullptr);
    if (n > 1) {
        std::string utf8(n - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, msg, -1, utf8.data(), n, nullptr, nullptr);
        DWORD written = 0;
        WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    }
    CloseHandle(h);
}

// Duplicate a 32-bit DIB while preserving its top-down/bottom-up orientation.
// CopyImage(LR_CREATEDIBSECTION) always produces bottom-up output, which flips
// the icons (cloud-badge overlays produced by GetShellBitmapByExtension are
// top-down 32-bit DIBs).  This helper reads the source DIBSECTION, creates a
// fresh DIBSECTION with the same biHeight sign, and memcpy's the pixels.
static HBITMAP DuplicateTopDownDib32(HBITMAP src) {
    if (!src) return nullptr;
    DIBSECTION ds = {};
    if (GetObjectW(src, sizeof(ds), &ds) != sizeof(ds)) {
        // Not a DIB section — fall back to CopyImage.
        return (HBITMAP)CopyImage(src, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    }
    BITMAPINFO bi = {};
    bi.bmiHeader = ds.dsBmih;
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biSizeImage   = 0;
    // Preserve orientation: top-down source has negative height, keep that sign.
    // GetObject reports biHeight as the absolute value with orientation info
    // elsewhere; detect via dsBm.bmBits alignment isn't reliable, so use the
    // original header's sign directly.
    bi.bmiHeader.biHeight = ds.dsBmih.biHeight;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP dst = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!dst || !bits) { if (dst) DeleteObject(dst); return nullptr; }

    const int absH = ds.dsBm.bmHeight;  // always positive
    const int width = ds.dsBm.bmWidth;
    const int bpp   = ds.dsBm.bmBitsPixel;
    const size_t stride = ((width * bpp + 31) / 32) * 4;
    memcpy(bits, ds.dsBm.bmBits, stride * absH);
    return dst;
}

// Create an HICON with proper 32-bit alpha at a specific size by drawing
// a larger source icon into a 32-bit DIB section, then wrapping it with
// CreateIconIndirect.  This bypasses LoadImageW's downsampling which
// can drop the alpha channel under MSIX packaged apps.
static HICON CreateAlphaIconAtSize(HINSTANCE hInst, LPCWSTR resId, int size) {
    // Load the source at 256x256 with LR_CREATEDIBSECTION (preserves alpha)
    HICON hSrc = (HICON)LoadImageW(hInst, resId, IMAGE_ICON, 256, 256, LR_CREATEDIBSECTION);
    if (!hSrc)
        hSrc = (HICON)LoadImageW(hInst, resId, IMAGE_ICON, 0, 0, LR_CREATEDIBSECTION);
    if (!hSrc) return nullptr;

    // Create a 32-bit top-down DIB section at the target size
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = size;
    bi.bmiHeader.biHeight      = -size;   // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hbmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC mem = CreateCompatibleDC(hdc);
    if (!hbmp || !bits || !mem) {
        if (hbmp) DeleteObject(hbmp);
        if (mem)  DeleteDC(mem);
        ReleaseDC(nullptr, hdc);
        DestroyIcon(hSrc);
        return nullptr;
    }
    HBITMAP old = (HBITMAP)SelectObject(mem, hbmp);

    // Clear to transparent black
    ZeroMemory(bits, size * size * 4);

    // Draw the source icon scaled down with alpha
    DrawIconEx(mem, 0, 0, hSrc, size, size, 0, nullptr, DI_NORMAL);
    GdiFlush();

    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(nullptr, hdc);
    DestroyIcon(hSrc);

    // Create a mask bitmap (all zero = no clipping, rely on alpha)
    BITMAPINFO mi = {};
    mi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    mi.bmiHeader.biWidth       = size;
    mi.bmiHeader.biHeight      = size;   // bottom-up for mask
    mi.bmiHeader.biPlanes      = 1;
    mi.bmiHeader.biBitCount    = 1;
    mi.bmiHeader.biCompression = BI_RGB;
    void* maskBits = nullptr;
    HDC maskDC = CreateCompatibleDC(nullptr);
    HBITMAP hMask = CreateDIBSection(maskDC, &mi, DIB_RGB_COLORS, &maskBits, nullptr, 0);
    if (hMask && maskBits)
        ZeroMemory(maskBits, ((size + 31) / 32) * 4 * size);  // DWORD-aligned DIB stride
    DeleteDC(maskDC);

    // Create the icon from the color bitmap + mask
    ICONINFO ii = {};
    ii.fIcon    = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask  = hMask;      // null is valid: CreateIconIndirect falls back to no mask
    ii.hbmColor = hbmp;

    HICON hIcon = CreateIconIndirect(&ii);

    if (hMask) DeleteObject(hMask);
    DeleteObject(hbmp);
    return hIcon;
}

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
// Messages carrying a raw pointer in lParam are restricted to the WM_APP
// range (0x8000+), reserved for application-private use. These window classes
// are findable by name (L"FanFolderMain", L"FanFolderFan"), so a WM_USER id
// would let any same-session process post a chosen lParam and have us
// reinterpret_cast it to a struct we then delete.
static constexpr UINT WM_MAIN_PREWARM      = WM_APP + 1;
// Posted by FolderWatcher's worker thread.  Carries no payload, so a forged
// message can at most trigger a harmless stamp check.
static constexpr UINT WM_MAIN_FOLDER_CHANGED = WM_APP + 4;
static constexpr UINT WM_MAIN_CLOSE_FAN    = WM_USER + 4;
// Sent BY the fan window to its owner when the fan wants to close itself
// (context-menu command completed, or an item was dragged out to Explorer).
// Deliberately distinct from WM_MAIN_CLOSE_FAN: that message is posted by the
// hook thread and carries a hook generation in wParam which the handler
// validates before acting. These posts pass wParam=0, so sharing the ID made
// the generation check fail and the close was silently dropped.
static constexpr UINT WM_MAIN_FAN_CLOSED   = WM_USER + 6;
static constexpr UINT WM_TRAYICON          = WM_USER + 5;   // tray icon messages
static constexpr int  TOGGLE_COOLDOWN_MS   = 250;

// Provenance handoff for WM_MAIN_PREWARM, same reasoning as IconHandoff in
// FanWindow.cpp: lParam carries a token, never a pointer, so a forged message
// from another process cannot make us dereference or delete an address of its
// choosing.
namespace PrewarmHandoff {
    std::mutex g_mutex;
    std::unordered_map<UINT, MainWindow::PrewarmData*> g_pending;
    UINT g_nextToken = 1;

    UINT Register(MainWindow::PrewarmData* p) {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_pending.size() >= 64) return 0;   // a handful of scans is the real range
        UINT token = g_nextToken++;
        if (g_nextToken == 0) g_nextToken = 1;
        g_pending.emplace(token, p);
        return token;
    }

    MainWindow::PrewarmData* Take(UINT token) {
        if (!token) return nullptr;
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_pending.find(token);
        if (it == g_pending.end()) return nullptr;
        auto* p = it->second;
        g_pending.erase(it);
        return p;
    }

    void Forget(UINT token) {
        if (!token) return;
        std::lock_guard<std::mutex> lk(g_mutex);
        g_pending.erase(token);
    }
}

// Timer id for periodic virtual-desktop reconciliation (see ReconcileVirtualDesktop).
static constexpr UINT_PTR TIMER_ID_VDM_RECONCILE = 1;
static constexpr UINT     TIMER_VDM_INTERVAL_MS  = 400;
// Coalesces a burst of filesystem notifications (copying a folder, extracting
// an archive) into one rescan.
static constexpr UINT_PTR TIMER_ID_FOLDER_REFRESH = 2;
static constexpr UINT     TIMER_FOLDER_REFRESH_MS  = 500;
// Fallback freshness check for shares and cloud folders where the OS watcher
// can miss notifications.  The directory walk runs on the threadpool, never on
// the UI thread.
static constexpr UINT_PTR TIMER_ID_FOLDER_VERIFY = 3;
static constexpr UINT     TIMER_FOLDER_VERIFY_MS  = 60000;
// Drives the scanning ring on the taskbar button and tray icon.  220 ms per
// frame is fast enough to read as motion and slow enough not to burn CPU on
// WM_SETICON / NIM_MODIFY round-trips.
static constexpr UINT_PTR TIMER_ID_SCAN_ANIM = 4;
static constexpr UINT     TIMER_SCAN_ANIM_MS = 220;
// A folder read on a small folder can finish inside one frame interval. Keep
// the ring on screen for at least one full turn so a click that lands during a
// scan always shows something.
static constexpr DWORD    BUSY_MIN_VISIBLE_MS = 900;

// Resource ids of the four scanning frames: transparent artwork for the
// taskbar button, folded-plate artwork for the tray, and a standalone ring
// badge for the taskbar overlay. Must match resource.h.
static constexpr WORD kTaskbarScanIds[4] = {
    IDI_APP_OPEN_SCAN_0, IDI_APP_OPEN_SCAN_1,
    IDI_APP_OPEN_SCAN_2, IDI_APP_OPEN_SCAN_3 };
static constexpr WORD kTrayScanIds[4] = {
    IDI_APP_SCAN_0, IDI_APP_SCAN_1, IDI_APP_SCAN_2, IDI_APP_SCAN_3 };
static constexpr WORD kOverlayScanIds[4] = {
    IDI_SCAN_OVERLAY_0, IDI_SCAN_OVERLAY_1,
    IDI_SCAN_OVERLAY_2, IDI_SCAN_OVERLAY_3 };

// Broadcast by the shell once the taskbar button for a window exists. Any
// ITaskbarList3 call made before it arrives is ignored.
static UINT TaskbarButtonCreatedMsg() {
    static const UINT id = RegisterWindowMessageW(L"TaskbarButtonCreated");
    return id;
}

MainWindow* MainWindow::s_instance = nullptr;

// ---------------------------------------------------------------------------
void MainWindow::Register(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = CreateAlphaIconAtSize(hInst, MAKEINTRESOURCEW(IDI_APP), 32);
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
    // Write the name index back so the next run starts warm. Best effort: a
    // failed write only costs one rebuild.
    FileService::SaveSearchIndex();

    // Join the watcher before the window disappears: its worker may still be
    // blocked in ReadDirectoryChangesW and must be woken by the stop event.
    _folderWatcher.Stop();
    RemoveTrayIcon();
    UninstallHooks();
    if (_hookThread.joinable()) {
        if (DWORD hookThreadId = _hookThreadId.load())
            PostThreadMessageW(hookThreadId, WM_QUIT, 0, 0);
        _hookThread.join();
    }
    if (_hwnd) {
        KillTimer(_hwnd, TIMER_ID_VDM_RECONCILE);
        KillTimer(_hwnd, TIMER_ID_FOLDER_REFRESH);
        KillTimer(_hwnd, TIMER_ID_FOLDER_VERIFY);
        KillTimer(_hwnd, TIMER_ID_SCAN_ANIM);
    }
    if (_vdm) {
        _vdm->Release();
        _vdm = nullptr;
    }
    // Release the taskbar/tray icons. RemoveTrayIcon() only detaches the shell
    // notification, so these four owned HICONs are freed here.
    if (_icoSmall)     { DestroyIcon(_icoSmall);     _icoSmall     = nullptr; }
    if (_icoBig)       { DestroyIcon(_icoBig);       _icoBig       = nullptr; }
    if (_icoOpenSmall) { DestroyIcon(_icoOpenSmall); _icoOpenSmall = nullptr; }
    if (_icoOpenBig)   { DestroyIcon(_icoOpenBig);   _icoOpenBig   = nullptr; }
    for (auto& h : _icoScanSmall) { if (h) DestroyIcon(h); h = nullptr; }
    for (auto& h : _icoScanBig)   { if (h) DestroyIcon(h); h = nullptr; }
    for (auto& h : _icoTrayScan)  { if (h) DestroyIcon(h); h = nullptr; }
    for (auto& h : _icoOverlay16) { if (h) DestroyIcon(h); h = nullptr; }
    for (auto& h : _icoOverlay32) { if (h) DestroyIcon(h); h = nullptr; }
    if (_taskbar) { _taskbar->Release(); _taskbar = nullptr; }
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

    // Set taskbar icon from embedded resource.
    // Under MSIX packaged apps, LoadImageW with explicit small sizes (16/32)
    // can drop the 32-bit alpha channel even with LR_CREATEDIBSECTION,
    // causing the icon background to appear as a solid blue square.
    // Fix: load the icon at native 256x256 size (which preserves alpha),
    // draw it into a 32-bit DIB section at the target size, then wrap
    // with CreateIconIndirect. This produces a proper alpha-aware HICON.
    _icoSmall     = CreateAlphaIconAtSize(_hInst, MAKEINTRESOURCEW(IDI_APP),     16);
    _icoBig       = CreateAlphaIconAtSize(_hInst, MAKEINTRESOURCEW(IDI_APP),     32);
    _icoOpenSmall = CreateAlphaIconAtSize(_hInst, MAKEINTRESOURCEW(IDI_APP_OPEN), 16);
    _icoOpenBig   = CreateAlphaIconAtSize(_hInst, MAKEINTRESOURCEW(IDI_APP_OPEN), 32);

    // Scanning frames. Taskbar gets the transparent artwork (same silhouette as
    // the open icon), the tray keeps the folded-plate style it already uses.
    for (int i = 0; i < kScanFrames; ++i) {
        _icoScanSmall[i] = CreateAlphaIconAtSize(
            _hInst, MAKEINTRESOURCEW(kTaskbarScanIds[i]), 16);
        _icoScanBig[i] = CreateAlphaIconAtSize(
            _hInst, MAKEINTRESOURCEW(kTaskbarScanIds[i]), 32);
        _icoTrayScan[i] = CreateAlphaIconAtSize(
            _hInst, MAKEINTRESOURCEW(kTrayScanIds[i]), 16);
        _icoOverlay16[i] = CreateAlphaIconAtSize(
            _hInst, MAKEINTRESOURCEW(kOverlayScanIds[i]), 16);
        _icoOverlay32[i] = CreateAlphaIconAtSize(
            _hInst, MAKEINTRESOURCEW(kOverlayScanIds[i]), 32);
    }

    InitTaskbarList();
    SetTaskbarIcon(false);

    StartPrewarm(/*force*/ true);
    StartFolderWatch();
    AddTrayIcon();

    // Virtual-desktop reconciliation.  The hidden main window is bound to
    // whatever virtual desktop was active at creation time.  Clicking the
    // taskbar icon from any other desktop would otherwise cause Windows to
    // switch desktops (to bring our window into view) before we can show the
    // fan — the user's click appears to "teleport" them away.  We can't react
    // inside SC_RESTORE because the desktop switch has already happened by
    // then, so we poll on a short timer and keep the window on whichever
    // desktop the user is currently looking at.
    if (SUCCEEDED(CoCreateInstance(CLSID_VirtualDesktopManager, nullptr,
                                   CLSCTX_ALL, IID_PPV_ARGS(&_vdm)))) {
        SetTimer(_hwnd, TIMER_ID_VDM_RECONCILE, TIMER_VDM_INTERVAL_MS, nullptr);
    }

    // First-launch info panel: show once on the very first run, or on every
    // launch when the "Show info on launch" tray toggle is enabled.  Persist
    // the first-run marker so the panel never auto-pops a second time for
    // users who ignore both the tray toggle and the config.
    if (!_config.infoScreenShown || _config.showInfoOnLaunch) {
        if (!_config.infoScreenShown) {
            _config.infoScreenShown = true;
            Config::Save(_config);
        }
        ShowInfoScreen();
    }

    return true;
}

// Move our hidden main window to the user's current virtual desktop if it's
// not already there.  Called from a WM_TIMER tick (~400 ms).  Cheap when the
// window is already on the current desktop (a single IsWindowOnCurrent… call).
void MainWindow::ReconcileVirtualDesktop() {
    if (!_vdm || !_hwnd) return;

    BOOL onCurrent = FALSE;
    if (FAILED(_vdm->IsWindowOnCurrentVirtualDesktop(_hwnd, &onCurrent))) return;
    if (onCurrent) return;

    // We're on a different desktop than the user.  We need the current
    // desktop's GUID, which the public API only exposes via an existing
    // window.  The foreground window — whatever the user is actually looking
    // at — is always on the current desktop by definition.
    HWND fg = GetForegroundWindow();
    if (!fg || fg == _hwnd) return;

    GUID currentId = {};
    if (FAILED(_vdm->GetWindowDesktopId(fg, &currentId))) return;
    if (IsEqualGUID(currentId, GUID{})) return;

    _vdm->MoveWindowToDesktop(_hwnd, currentId);
}

// ---------------------------------------------------------------------------
void MainWindow::ToggleFan() {
    DWORD now = GetTickCount();
    DebugLog((L"[FanFolder] ToggleFan: now=" + std::to_wstring(now) +
        L" last=" + std::to_wstring(_lastToggleTick) +
        L" diff=" + std::to_wstring(now - _lastToggleTick) +
        L" fanOpen=" + std::to_wstring((int)(bool)_fanOpen) +
        L" hookClose=" + std::to_wstring(now - _hookCloseTick) + L"\n").c_str());
    if (now - _lastToggleTick < TOGGLE_COOLDOWN_MS) {
        DebugLog(L"[FanFolder] ToggleFan: COOLDOWN — skipped\n");
        return;
    }
    _lastToggleTick = now;

    if (_fanOpen) {
        CloseFan();
    } else if (now - _hookCloseTick < 600) {
        // The mouse/keyboard hook JUST closed the fan; this SC_RESTORE is from
        // the same physical click that triggered the hook.  Don't re-open.
        DebugLog(L"[FanFolder] ToggleFan: suppressed re-open (hook just closed)\n");
    } else {
        OpenFan();
    }
}

void MainWindow::OpenFan() {
    DWORD tEnter = GetTickCount();
    DebugLog((L"[FanFolder] OpenFan: ENTER tick=" + std::to_wstring(tEnter) + L"\n").c_str());

    DWORD t0 = GetTickCount();
    UninstallHooks();
    if (_fanWindow) {
        _fanWindow->Close();
        _fanWindow.reset();
    }
    RefreshBusyIndicator();   // a fan search may have been the only busy source
    DWORD tTeardown = GetTickCount() - t0;

    // Copy pre-warmed data (items + icons) under the lock. We duplicate the
    // icon handles rather than moving them so _prewarm stays ready for rapid
    // reopens — otherwise the second open would hit "no prewarm" and spawn
    // 15 concurrent icon workers that stall the UI on GDI+ contention.
    DWORD tLock0 = GetTickCount();
    std::vector<FileItem> prewarmItems;
    std::vector<HBITMAP>  prewarmBitmaps;
    std::vector<HICON>    prewarmIcons;
    std::vector<std::shared_ptr<Gdiplus::Bitmap>> prewarmGdiBitmaps;
    int                   prewarmIconSize = 0;
    bool havePrewarm = false;
    {
        std::lock_guard<std::mutex> lk(_prewarmMutex);
        if (_prewarm.ready) {
            prewarmItems    = _prewarm.items;
            prewarmIconSize = _prewarm.iconSize;
            prewarmBitmaps.reserve(_prewarm.bitmaps.size());
            prewarmIcons.reserve(_prewarm.icons.size());
            for (auto h : _prewarm.bitmaps) {
                prewarmBitmaps.push_back(h ? DuplicateTopDownDib32(h) : nullptr);
            }
            for (auto h : _prewarm.icons) {
                prewarmIcons.push_back(h ? CopyIcon(h) : nullptr);
            }
            // shared_ptr copy = cheap refcount bump; FanWindow and _prewarm
            // now share the same Gdiplus::Bitmap objects, lifetime-safe even
            // if _prewarm is replaced by a newer post while the fan is open.
            prewarmGdiBitmaps = _prewarm.gdiBitmaps;
            havePrewarm = true;
            // _prewarm stays ready & intact — next OpenFan can reuse it.
        }
    }
    DWORD tLock = GetTickCount() - tLock0;

    // Never block the UI thread with ScanFolder — use cached items if prewarm isn't ready
    std::vector<FileItem> items;
    if (havePrewarm) {
        items = std::move(prewarmItems);
        DebugLog((L"[FanFolder] OpenFan: using prewarm (dup), " + std::to_wstring(items.size()) + L" items\n").c_str());
    } else if (!_cachedItems.empty()) {
        items = _cachedItems;
        DebugLog((L"[FanFolder] OpenFan: using cached, " + std::to_wstring(items.size()) + L" items\n").c_str());
    } else {
        // Prewarm from the ctor / previous CloseFan is still in flight.
        // Do NOT call StartPrewarm() here — it would bump _prewarmGen and
        // cancel the in-flight worker, causing an infinite cancel loop if the
        // user keeps clicking rapidly at startup.
        DebugLog(L"[FanFolder] OpenFan: NO DATA — waiting for in-flight prewarm\n");
        return;
    }

    // The watcher normally keeps the prewarm current, but a change can race
    // with the click.  Do not check the folder here: even a directory walk
    // would add latency to every open.  The watcher and the background verifier
    // catch the change; an open fan is refreshed when the scan completes.
    // Drain any stale close messages queued by old (detached) hook threads
    MSG drain;
    while (PeekMessageW(&drain, _hwnd, WM_MAIN_CLOSE_FAN, WM_MAIN_CLOSE_FAN, PM_REMOVE)) {}

    // Second-pass GDI+ warm-up on first fan open of this session:
    // Convert 2-3 real icon items to GDI+ bitmaps to warm up the icon conversion
    // pipeline. This reduces stutter/slowness on the very first open after wake/reboot.
    if (!_gdiPlusFullyWarmed.load(std::memory_order_acquire) && !prewarmBitmaps.empty()) {
        int numToWarmup = std::min(3, (int)prewarmBitmaps.size());
        for (int i = 0; i < numToWarmup; ++i) {
            if (!prewarmBitmaps[i]) continue;
            // Convert HBITMAP to Gdiplus::Bitmap once to warm the conversion path.
            // The result is deliberately discarded: FanWindow::Show() converts the
            // same bitmaps again, fast now that the codecs and device contexts are
            // warm.  It still has to be deleted here or it leaks on the first open
            // of every session.
            if (Gdiplus::Bitmap* warm = FanWindow::HBitmapToGdiBitmap(prewarmBitmaps[i]))
                delete warm;
        }
        _gdiPlusFullyWarmed.store(true, std::memory_order_release);
        DebugLog(L"[FanFolder] OpenFan: second-pass GDI+ warm-up complete\n");
    }

    DWORD tCtor0 = GetTickCount();
    _fanWindow = std::make_unique<FanWindow>(_hInst, _hwnd, _config, std::move(items));
    DWORD tCtor = GetTickCount() - tCtor0;

    // Inject pre-warmed icons; FanWindow::Show() will skip async loading for them
    if (havePrewarm)
        _fanWindow->AcceptPrewarmIcons(std::move(prewarmBitmaps),
                                       std::move(prewarmIcons),
                                       std::move(prewarmGdiBitmaps),
                                       prewarmIconSize);

    DWORD tCreate0 = GetTickCount();
    bool created = _fanWindow->Create();
    DWORD tCreate = GetTickCount() - tCreate0;

    if (created) {
        DWORD tShow0 = GetTickCount();
        _fanWindow->Show();
        DWORD tShow = GetTickCount() - tShow0;
        _fanOpen     = true;
        _fanOpenTick = GetTickCount();
        DWORD tIcon0 = GetTickCount();
        SetTaskbarIcon(true);
        DWORD tIcon = GetTickCount() - tIcon0;
        DWORD tHook0 = GetTickCount();
        InstallHooks();
        DWORD tHook = GetTickCount() - tHook0;

        DWORD tTotal = GetTickCount() - tEnter;
        DebugLog((L"[FanFolder] OpenFan: timing teardown=" + std::to_wstring(tTeardown) +
                  L" lock=" + std::to_wstring(tLock) +
                  L" ctor=" + std::to_wstring(tCtor) +
                  L" create=" + std::to_wstring(tCreate) +
                  L" show=" + std::to_wstring(tShow) +
                  L" icon=" + std::to_wstring(tIcon) +
                  L" hook=" + std::to_wstring(tHook) +
                  L" TOTAL=" + std::to_wstring(tTotal) + L"ms\n").c_str());
    } else {
        _fanWindow.reset();
        _fanOpen = false;
    }
}

// Off-UI freshness check.  The caller only snapshots the current provenance
// and hands the directory walk to the threadpool, so closing the fan never
// waits on disk I/O.  A stamp mismatch posts WM_MAIN_FOLDER_CHANGED, which
// starts the debounced rescan.
void MainWindow::VerifyFolderChangedAsync() {
    if (FileService::IsVirtualSentinel(_config.folderPath))
        return;

    ConfigData cfg;
    FILETIME expected = {};
    {
        std::lock_guard<std::mutex> lk(_prewarmMutex);
        if (!_prewarm.ready || !_prewarm.stampValid ||
            !(_prewarm.scanConfig == _config))
            return;
        cfg      = _prewarm.scanConfig;
        expected = _prewarm.folderStamp;
    }

    struct VerifyWork {
        ConfigData cfg;
        FILETIME   expected;
        HWND       hwnd;
        UINT       msg;
    };
    auto* work = new VerifyWork{std::move(cfg), expected, _hwnd, WM_MAIN_FOLDER_CHANGED};

    BOOL submitted = TrySubmitThreadpoolCallback([](PTP_CALLBACK_INSTANCE, PVOID ctx) {
        std::unique_ptr<VerifyWork> w(static_cast<VerifyWork*>(ctx));
        try {
            // The index stamps every directory, so this is one stat per
            // directory instead of a walk of the whole tree. Only when there is
            // no index at all does the cheap root comparison run.
            bool fresh = FileService::IndexIsFresh();
            if (!fresh) {
                FILETIME now = {};
                if (FileService::ReadFolderStamp(w->cfg.folderPath, now,
                                                 /*recursive=*/false)) {
                    fresh = (now.dwLowDateTime  == w->expected.dwLowDateTime &&
                             now.dwHighDateTime == w->expected.dwHighDateTime);
                }
            }
            if (!fresh)
                PostMessageW(w->hwnd, w->msg, 0, 0);
        } catch (...) {
            // A verifier failure is not fatal: the next close or timer tick
            // tries again, and the watcher covers normal local-folder changes.
        }
    }, work, nullptr);

    if (!submitted)
        delete work;
}

// Start (or restart) the filesystem watch for the selected folder.  The
// watcher follows subdirectories only when IncludeSubfolderFiles is on, so the
// notification volume matches what the scan actually shows.
void MainWindow::StartFolderWatch() {
    if (!_hwnd) return;

    // A new watch means a new folder, resume, or a settings change: queued paths
    // from the previous watch must not be applied to the new index.
    {
        std::lock_guard<std::mutex> lk(_prewarmMutex);
        _pendingIndexChanges.clear();
    }

    const bool isVirtual = FileService::IsVirtualSentinel(_config.folderPath);
    _folderWatcher.Start(_config.folderPath, _config.includeSubfolderFiles,
                         _hwnd, WM_MAIN_FOLDER_CHANGED);

    // A virtual sentinel has no directory to stamp, so the periodic verifier
    // has nothing to do.  For a real folder it is a safety net, not the main
    // trigger; the watcher handles normal changes immediately.
    if (isVirtual)
        KillTimer(_hwnd, TIMER_ID_FOLDER_VERIFY);
    else
        SetTimer(_hwnd, TIMER_ID_FOLDER_VERIFY, TIMER_FOLDER_VERIFY_MS, nullptr);
}

// Debounced handler for a filesystem notification.  Skip the scan while one is
// already running; its completion will schedule another pass if changes
// arrived during the scan.  StartPrewarm only queues a worker, so this returns
// without waiting on the folder.
void MainWindow::RefreshFromFolderChange() {
    if (!_folderDirty.exchange(false, std::memory_order_acq_rel))
        return;

    if (_prewarmInflight.load(std::memory_order_acquire) > 0) {
        _folderDirty.store(true, std::memory_order_release);
        return;
    }

    StartPrewarm(/*force*/ true);
}

void MainWindow::CloseFan() {
    DebugLog(L"[FanFolder] CloseFan: ENTER\n");
    UninstallHooks();
    if (_fanWindow) {
        _fanWindow->Close();
        _fanWindow.reset();
    }
    _fanOpen = false;
    // The LL mouse hook also serves the info panel (outside-click dismissal);
    // re-install it if the panel is still visible.
    if (_infoScreen && _infoScreen->IsVisible())
        InstallHooks();
    RefreshBusyIndicator();   // dropping the fan drops its search state too
    SetTaskbarIcon(false);
    ShowWindow(_hwnd, SW_SHOWMINNOACTIVE);
    // Refresh so the fan reflects files added/removed/renamed while it was
    // open (e.g. via the right-click shell menu).  The check itself runs on a
    // threadpool worker: closing must not wait on a directory walk, and the
    // next open should not inherit a busy UI thread.
    VerifyFolderChangedAsync();
}

// ---------------------------------------------------------------------------
void MainWindow::SetTaskbarIcon(bool /*open*/) {
    // Single visual state regardless of whether the fan menu is shown: the
    // wide transparent icon, or its scanning ring while a scan is running.
    ApplyTaskbarIcon();
}

void MainWindow::ApplyTaskbarIcon() {
    if (_iconBusy && _icoScanSmall[_iconFrame] && _icoScanBig[_iconFrame]) {
        SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)_icoScanSmall[_iconFrame]);
        SendMessageW(_hwnd, WM_SETICON, ICON_BIG,   (LPARAM)_icoScanBig[_iconFrame]);
        return;
    }
    HICON sm = _icoOpenSmall ? _icoOpenSmall : _icoSmall;
    HICON lg = _icoOpenBig   ? _icoOpenBig   : _icoBig;
    if (sm) SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)sm);
    if (lg) SendMessageW(_hwnd, WM_SETICON, ICON_BIG,   (LPARAM)lg);
}

// The taskbar button on Windows 11 is grouped, and its icon comes from the
// shortcut / window icon the shell was told about when the button was created.
// WM_SETICON does not repaint that button, so the ring has to be applied as an
// ITaskbarList3 overlay as well. Both channels are driven from the same frame
// counter, which keeps Windows 10, Alt+Tab and the Win11 button consistent.
void MainWindow::InitTaskbarList() {
    if (_taskbar) return;
    if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&_taskbar))) || !_taskbar) {
        _taskbar = nullptr;
        return;
    }
    if (FAILED(_taskbar->HrInit())) {
        _taskbar->Release();
        _taskbar = nullptr;
    }
}

void MainWindow::ApplyOverlayIcon() {
    // Calls made before the button exists are ignored by the shell, never
    // harmful, and a later frame applies normally. TaskbarButtonCreated forces
    // one extra attempt for the case where the button appears mid-scan.
    if (!_taskbar || !_hwnd) return;

    if (_iconBusy) {
        const UINT dpi = GetDpiForWindow(_hwnd);
        HICON h = (dpi > 120 && _icoOverlay32[_iconFrame])
                      ? _icoOverlay32[_iconFrame]
                      : _icoOverlay16[_iconFrame];
        if (h) {
            _taskbar->SetOverlayIcon(_hwnd, h, L"Scanning");
            return;
        }
    }
    _taskbar->SetOverlayIcon(_hwnd, nullptr, nullptr);
}

void MainWindow::ApplyTrayIcon() {
    if (!_nid.cbSize) return;   // tray icon not added yet
    HICON h = (_iconBusy && _icoTrayScan[_iconFrame])
                  ? _icoTrayScan[_iconFrame]
                  : _icoSmall;
    if (!h || _nid.hIcon == h) return;
    _nid.hIcon  = h;
    _nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &_nid);
}

// A scan is "visible" for both kinds of wait the user can hit:
//   * folder prewarm, i.e. the fan's content is still being read from disk
//     (also covers the moment right after a click when no list is cached yet),
//   * the type-to-filter search walk, which keeps running after the provisional
//     in-memory matches are already on screen.
// While either is true the icon cycles its frames; when both settle it goes
// back to the idle artwork.
void MainWindow::RefreshBusyIndicator() {
    if (!_hwnd) return;

    bool raw = _prewarmInflight.load(std::memory_order_acquire) > 0;
    if (!raw && _fanWindow)
        raw = _fanWindow->IsSearchBusy();

    // Hold the ring for a moment after a fast scan so it is actually seen.
    const DWORD now = GetTickCount();
    if (raw) _busyHoldUntil = now + BUSY_MIN_VISIBLE_MS;
    const bool busy = raw || static_cast<LONG>(_busyHoldUntil - now) > 0;

    if (busy == _iconBusy) return;
    _iconBusy  = busy;
    _iconFrame = 0;

    if (busy) {
        SetTimer(_hwnd, TIMER_ID_SCAN_ANIM, TIMER_SCAN_ANIM_MS, nullptr);
    } else {
        KillTimer(_hwnd, TIMER_ID_SCAN_ANIM);
    }
    ApplyTaskbarIcon();
    ApplyTrayIcon();
    ApplyOverlayIcon();

    if (_iconBusy) {
        DebugLog(L"[FanFolder] busy indicator: ON\n");
    } else {
        DebugLog(L"[FanFolder] busy indicator: OFF\n");
    }
}

// ---------------------------------------------------------------------------
void MainWindow::StartPrewarm(bool force) {
    // Debounce: if a prewarm worker is already queued/running, skip unless the
    // caller forces a refresh (e.g. folder/settings actually changed).  Rapid
    // open/close cycles would otherwise queue a cascade of redundant scan +
    // icon-extract workers, each spawning 15 shell thumbnail lookups.
    if (!force && _prewarmInflight.load(std::memory_order_acquire) > 0)
        return;

    ConfigData cfg   = _config;
    HWND       hwnd  = _hwnd;
    int        myGen = ++_prewarmGen;  // invalidates any still-running prewarm thread

    // Watcher notifications are applied to the index on the worker, not here:
    // re-enumerating a directory on the UI thread would stall the menu if that
    // directory sits on a slow share.
    std::vector<FileService::IndexChange> changes;
    bool rebuild = false;
    {
        std::lock_guard<std::mutex> lk(_prewarmMutex);
        if (force) {
            changes.swap(_pendingIndexChanges);
            rebuild = _indexRebuildNeeded;
            _indexRebuildNeeded = false;
        }
    }

    struct PrewarmWork {
        MainWindow* self;
        ConfigData  cfg;
        HWND        hwnd;
        int         myGen;
        bool        force;
        bool        rebuild;
        std::vector<FileService::IndexChange> changes;
    };
    auto* work = new PrewarmWork{this, std::move(cfg), hwnd, myGen, force,
                                 rebuild, std::move(changes)};

    _prewarmInflight.fetch_add(1, std::memory_order_acq_rel);
    RefreshBusyIndicator();   // the menu content is being (re)scanned now

    BOOL submitted = TrySubmitThreadpoolCallback([](PTP_CALLBACK_INSTANCE, PVOID ctx) {
        // Own the work item for the callback's whole lifetime: the body has
        // several early returns plus a throw path, and a leak or a missed
        // decrement on any of them would strand _prewarmInflight and wedge
        // prewarming for the rest of the session.
        std::unique_ptr<PrewarmWork> workOwner(static_cast<PrewarmWork*>(ctx));
        PrewarmWork* pw = workOwner.get();

        // Nothing here may escape the callback. Scanning hostile Jump Lists and
        // loading icons can throw (bad_alloc, length_error), and an exception
        // crossing the threadpool boundary calls std::terminate.
        try {
        // Balanced COM teardown and inflight accounting on every exit path,
        // including a throw.
        struct ScopedPrewarmExit {
            MainWindow* self;
            bool comInited;
            ~ScopedPrewarmExit() {
                if (comInited) CoUninitialize();
                self->_prewarmInflight.fetch_sub(1, std::memory_order_acq_rel);
            }
        } exitGuard{ pw->self, SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) };

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

        // Apply the watcher notifications to the index before scanning, so a
        // normal save costs one directory instead of a walk of the tree. An
        // overflowed buffer, or a forced refresh with no details, rebuilds.
        if (pw->rebuild) {
            FileService::InvalidateSearchIndex();
        } else if (!pw->changes.empty()) {
            if (!FileService::ApplyIndexChanges(pw->cfg.folderPath, pw->cfg.includeDirs,
                                                pw->cfg.includeSubfolderFiles,
                                                pw->changes))
                FileService::InvalidateSearchIndex();
        } else if (pw->force) {
            FileService::InvalidateSearchIndex();
        }

        // Read the directory mtime BEFORE scanning.  Capturing it first means a
        // change that lands mid-scan still leaves us with the older stamp, so
        // the next open rescans rather than trusting a scan that raced.
        FILETIME stamp = {};
        // Root mtime only: the watcher covers changes below it, and the index
        // keeps its own per-directory stamps for the freshness check.
        const bool stampValid = FileService::ReadFolderStamp(
            pw->cfg.folderPath, stamp, /*recursive=*/false);

        // Build the name index here, on the prewarm worker, so the first fan
        // open and the first keystroke never wait for a tree walk. ScanFolder
        // below then serves the list straight out of the index.
        FileService::WarmSearchIndex(pw->cfg.folderPath, pw->cfg.includeDirs,
                                     pw->cfg.includeSubfolderFiles);

        auto items = FileService::ScanFolder(pw->cfg.folderPath, pw->cfg.maxItems,
                                             pw->cfg.includeDirs, pw->cfg.filterRegex,
                                             pw->cfg.sortMode, /*resolveLnk=*/true,
                                             pw->cfg.includeSubfolderFiles);
        auto* data      = new MainWindow::PrewarmData;
        data->items     = items;
        data->iconSize  = iconSize;
        data->ready     = true;
        data->scanConfig     = pw->cfg;
        data->folderStamp    = stamp;
        data->stampValid     = stampValid;
        data->bitmaps.resize(items.size(), nullptr);
        data->icons.resize(items.size(), nullptr);
        data->gdiBitmaps.resize(items.size());

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

            // Pre-convert to Gdiplus::Bitmap here (on the worker thread, while
            // the fan is not drawing).  Cached across fan opens so Show() no
            // longer pays the ~50ms × 15 = ~750ms HBitmapToGdiBitmap tax on the
            // UI thread every single open.  shared_ptr lifetime lets FanWindow
            // borrow these safely even if _prewarm is replaced by a newer
            // post while an open fan still holds them.
            if (data->bitmaps[i]) {
                data->gdiBitmaps[i].reset(FanWindow::HBitmapToGdiBitmap(data->bitmaps[i]));
            } else if (data->icons[i]) {
                data->gdiBitmaps[i].reset(Gdiplus::Bitmap::FromHICON(data->icons[i]));
            }
        }

        // Only post if still the current generation; discard stale results
        if (pw->myGen != pw->self->_prewarmGen.load()) {
            data->FreeHandles();
            delete data;
            return;   // exitGuard handles CoUninitialize, inflight and `delete pw`
        }
        data->gen = pw->myGen;
        const UINT token = PrewarmHandoff::Register(data);
        if (!token || !PostMessageW(pw->hwnd, WM_MAIN_PREWARM, 0, (LPARAM)token)) {
            // Window gone (or the registry was full) — free data ourselves so
            // HBITMAP/HICON/GDI+ bitmaps don't leak.
            PrewarmHandoff::Forget(token);
            data->FreeHandles();
            delete data;
        }
        } catch (...) {
            // Never let an exception escape a threadpool callback. exitGuard
            // still balances COM and the inflight counter, workOwner frees `pw`.
        }
    }, work, nullptr);

    if (!submitted) {
        _prewarmInflight.fetch_sub(1, std::memory_order_acq_rel);
        delete work;
    }
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
        // _nid.hIcon is a borrowed alias of _icoSmall (see AddTrayIcon), not an
        // owned copy, so it must not be destroyed here.  The four icons this class
        // does own are released in ~MainWindow.
        _nid = {};
    }
}

void MainWindow::ShowInfoScreen() {
    if (!_infoScreen) {
        _infoScreen = std::make_unique<InfoScreen>(_hInst);
        if (!_infoScreen->Create(_hwnd)) {
            _infoScreen.reset();
            return;
        }
    }
    // Outside-click dismissal is seen by the LL mouse hook, so it must be
    // running while the panel is visible (InstallHooks is a no-op when the
    // fan already installed it earlier in this session).
    InstallHooks();
    _infoScreen->Show();
}

void MainWindow::CloseInfoScreen() {
    if (_infoScreen) _infoScreen->Close();
    // Keep the hook alive if the fan is open (it has its own consumers);
    // otherwise shut it down as CloseFan would.
    if (!_fanOpen)
        UninstallHooks();
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
        ID_INCLUDE_SUBFOLDER_FILES,
        ID_SHOW_EXTENSIONS,
        ID_FILTER_AS_YOU_TYPE,
        ID_FOLDER_DOWNLOADS,
        ID_FOLDER_DESKTOP,
        ID_FOLDER_DOCUMENTS,
        ID_FOLDER_RECENTDOCS,
        ID_FOLDER_RECENTFILES,
        ID_FOLDER_GRAPHRECENT,
        ID_FOLDER_BROWSE,
        ID_VISIT_WEBPAGE,
        ID_SHOW_INFO_ON_LAUNCH,
        ID_EXIT,
    };

    const Strings& s = GetStrings();

    HMENU hSort = CreatePopupMenu();
    const bool isRecentMode = (_config.folderPath == L"::RecentDocs::" ||
                                _config.folderPath == L"::RecentFiles::" ||
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

    // Animation submenu. All six strings (s.animation plus one per style)
    // already exist in every one of the 29 locale tables, and README.md lists
    // the styles as a feature, so this needs no new localization.
    HMENU hAnim = CreatePopupMenu();
    AppendMenuW(hAnim, MF_STRING | (_config.animStyle == ConfigData::AnimStyle::Fan    ? MF_CHECKED : 0), ID_ANIM_FAN,    s.animFan);
    AppendMenuW(hAnim, MF_STRING | (_config.animStyle == ConfigData::AnimStyle::Glide  ? MF_CHECKED : 0), ID_ANIM_GLIDE,  s.animGlide);
    AppendMenuW(hAnim, MF_STRING | (_config.animStyle == ConfigData::AnimStyle::Spring ? MF_CHECKED : 0), ID_ANIM_SPRING, s.animSpring);
    AppendMenuW(hAnim, MF_STRING | (_config.animStyle == ConfigData::AnimStyle::Fade   ? MF_CHECKED : 0), ID_ANIM_FADE,   s.animFade);
    AppendMenuW(hAnim, MF_STRING | (_config.animStyle == ConfigData::AnimStyle::None   ? MF_CHECKED : 0), ID_ANIM_NONE,   s.animNone);

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
    AppendMenuW(hFolder, MF_STRING | (_config.folderPath == L"::RecentFiles::"  ? MF_CHECKED : 0), ID_FOLDER_RECENTFILES, s.folderRecentFiles);
    AppendMenuW(hFolder, MF_STRING | (_config.folderPath == L"::GraphRecent::" ? MF_CHECKED : 0), ID_FOLDER_GRAPHRECENT, s.folderGraphRecent);
    AppendMenuW(hFolder, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFolder, MF_STRING, ID_FOLDER_BROWSE, s.folderBrowse);

    // Build folder path label (truncated)
    std::wstring folderLabel = s.openPrefix;
    if (_config.folderPath == L"::RecentDocs::") {
        folderLabel += s.folderRecentDocs;
    } else if (_config.folderPath == L"::RecentFiles::") {
        folderLabel += s.folderRecentFiles;
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
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hAnim,   s.animation);
    AppendMenuW(hMenu, MF_STRING | (_config.includeDirs    ? MF_CHECKED : 0), ID_INCLUDE_DIRS,    s.includeFolders);
    AppendMenuW(hMenu,
        MF_STRING | (isRecentMode ? MF_GRAYED : 0) |
        (_config.includeSubfolderFiles ? MF_CHECKED : 0),
        ID_INCLUDE_SUBFOLDER_FILES, IncludeSubfolderFilesLabel());
    AppendMenuW(hMenu, MF_STRING | (_config.showExtensions ? MF_CHECKED : 0), ID_SHOW_EXTENSIONS, s.showExtensions);
    AppendMenuW(hMenu, MF_STRING | (_config.filterAsYouType ? MF_CHECKED : 0),
                ID_FILTER_AS_YOU_TYPE, FilterAsYouTypeLabel());
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hFolder, s.folderSubmenu);
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | (_config.showInfoOnLaunch ? MF_CHECKED : 0), ID_SHOW_INFO_ON_LAUNCH, GetInfoStrings().trayItem);
    AppendMenuW(hMenu, MF_STRING, ID_VISIT_WEBPAGE, s.visitWebpage);
    AppendMenuW(hMenu, MF_STRING, ID_EXIT, s.exitApp);

    // Position menu at cursor; SetForegroundWindow required for proper dismissal
    POINT pt = {};
    GetCursorPos(&pt);
    SetForegroundWindow(_hwnd);
    int cmd = (int)TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                                    pt.x, pt.y, _hwnd, nullptr);
    DestroyMenu(hMenu);

    if (!cmd) return;

    const std::wstring oldFolderPath = _config.folderPath;
    const bool oldIncludeSubfolderFiles = _config.includeSubfolderFiles;
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
    case ID_INCLUDE_SUBFOLDER_FILES: _config.includeSubfolderFiles = !_config.includeSubfolderFiles; break;
    case ID_SHOW_EXTENSIONS:_config.showExtensions = !_config.showExtensions; break;
    case ID_FILTER_AS_YOU_TYPE: _config.filterAsYouType = !_config.filterAsYouType; break;
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
    case ID_FOLDER_RECENTFILES:
        _config.folderPath = L"::RecentFiles::";
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
    case ID_VISIT_WEBPAGE:
        changed = false;
        ShellExecuteW(nullptr, L"open", L"https://olebhartvigsen.github.io/FanFolder/",
                      nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case ID_SHOW_INFO_ON_LAUNCH:
        _config.showInfoOnLaunch = !_config.showInfoOnLaunch;
        Config::Save(_config);   // persist immediately; no folder rescan needed
        changed = false;
        break;
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
        StartPrewarm(/*force*/ true);
        // The watch parameters only change with the folder and the subfolder
        // toggle; sort/filter/limit changes do not need a watcher restart.
        if (_config.folderPath != oldFolderPath ||
            _config.includeSubfolderFiles != oldIncludeSubfolderFiles) {
            StartFolderWatch();
        }
    }
}

// ---------------------------------------------------------------------------
static thread_local int t_hookGen = 0;  // each hook thread captures its generation

void MainWindow::InstallHooks() {
    if (_hookThread.joinable()) return;

    int gen  = ++_hookGen;
    HWND hwnd = _hwnd;
    _hookThread = std::thread([this, hwnd, gen]() {
        _hookThreadId = GetCurrentThreadId();
        t_hookGen     = gen;  // stamp this thread with its generation

        HHOOK mouseHook = SetWindowsHookExW(WH_MOUSE_LL,    MouseHookProc,    nullptr, 0);
        HHOOK kbHook    = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, nullptr, 0);
        HWINEVENTHOOK winEvent = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                      nullptr, WinEventProc, 0, 0,
                                      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (mouseHook) UnhookWindowsHookEx(mouseHook);
        if (kbHook)    UnhookWindowsHookEx(kbHook);
        if (winEvent)  UnhookWinEvent(winEvent);
    });
}

void MainWindow::UninstallHooks() {
    if (_hookThread.joinable()) {
        if (DWORD hookThreadId = _hookThreadId.load())
            PostThreadMessageW(hookThreadId, WM_QUIT, 0, 0);
        _hookThread.detach();  // never block the UI thread; hooks will self-cleanup on WM_QUIT
        _hookThreadId.store(0);
    }
}

// ---------------------------------------------------------------------------
LRESULT CALLBACK MainWindow::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // Info panel (first launch / "show info on launch"): a click that does not
    // land on the panel's card dismisses it.  Never swallows the click, so
    // whatever window is underneath still receives it (standard click-away).
    if (nCode >= 0 && s_instance && s_instance->_infoScreen &&
        s_instance->_infoScreen->IsVisible()) {
        bool isDown = (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
                       wParam == WM_MBUTTONDOWN);
        if (isDown) {
            POINT pt = {};
            GetCursorPos(&pt);
            if (!s_instance->_infoScreen->IsPointOnPanel(pt)) {
                PostMessageW(s_instance->_hwnd, InfoScreen::WM_INFO_DISMISSED,
                             (WPARAM)t_hookGen, 0);
                // fall through to the fan branch below (panel + fan may both be up)
            }
        }
    }

    if (nCode >= 0 && s_instance && s_instance->_fanOpen) {
        bool isDown = (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
                       wParam == WM_MBUTTONDOWN);
        if (isDown && s_instance->_fanWindow) {
            HWND fanHwnd = s_instance->_fanWindow->Handle();
            if (fanHwnd) {
                POINT pt = {};
                GetCursorPos(&pt);
                RECT rc = {};
                GetWindowRect(fanHwnd, &rc);
                bool outside = (pt.x < rc.left || pt.x >= rc.right ||
                                pt.y < rc.top  || pt.y >= rc.bottom);
                // If the click lands on the taskbar, don't close the fan here.
                // The taskbar will post SC_RESTORE to our main window, which will
                // toggle-close the fan cleanly.  Closing here AND then processing
                // SC_RESTORE causes a restore→minimize roundtrip on the main
                // window that stalls the next reopen's rendering for ~5s.
                bool onTaskbar = false;
                if (outside) {
                    HWND hit = WindowFromPoint(pt);
                    for (HWND w = hit; w; w = GetAncestor(w, GA_PARENT)) {
                        wchar_t cls[64] = {};
                        GetClassNameW(w, cls, 64);
                        if (wcscmp(cls, L"Shell_TrayWnd") == 0 ||
                            wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0 ||
                            wcscmp(cls, L"WorkerW") == 0) {
                            onTaskbar = true;
                            break;
                        }
                        if (w == GetDesktopWindow()) break;
                    }
                }
                DWORD elapsed = GetTickCount() - s_instance->_fanOpenTick;
                DebugLog((L"[FanFolder] MouseHook: wParam=" +
                    std::to_wstring(wParam) +
                    L" pt=(" + std::to_wstring(pt.x) + L"," + std::to_wstring(pt.y) + L")" +
                    L" outside=" + std::to_wstring(outside) +
                    L" onTaskbar=" + std::to_wstring(onTaskbar) +
                    L" elapsed=" + std::to_wstring(elapsed) + L"ms\n").c_str());
                if (outside && !onTaskbar) {
                    PostMessageW(s_instance->_hwnd, WM_MAIN_CLOSE_FAN, (WPARAM)t_hookGen, 1/*mouse*/);
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK MainWindow::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && s_instance && s_instance->_fanOpen && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        UINT vk = kb->vkCode;

        // Escape — if a filter is active, forward to the fan to clear it
        // (fan stays open).  If no filter, close the fan (existing behavior).
        if (vk == VK_ESCAPE) {
            HWND fanHwnd = (s_instance->_fanWindow) ? s_instance->_fanWindow->Handle() : nullptr;
            if (fanHwnd && s_instance->_fanWindow->IsFilterActive()) {
                PostMessageW(fanHwnd, FanWindow::WM_FAN_FILTER_KEY, (WPARAM)VK_ESCAPE, 0);
                return 1;  // swallow — don't leak to the app underneath
            }
            PostMessageW(s_instance->_hwnd, WM_MAIN_CLOSE_FAN, (WPARAM)t_hookGen, 2/*keyboard*/);
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        // Phase 4: when filterAsYouType is disabled, don't forward any
        // printable/Backspace/Enter keys — the fan behaves as it did before
        // the feature (all keystrokes pass through to the app underneath).
        if (!s_instance->_config.filterAsYouType)
            return CallNextHookEx(nullptr, nCode, wParam, lParam);

        // Only forward when the fan window exists and is visible.
        HWND fanHwnd = (s_instance->_fanWindow) ? s_instance->_fanWindow->Handle() : nullptr;
        if (fanHwnd && IsWindow(fanHwnd) && IsWindowVisible(fanHwnd)) {
            // Backspace — forward as a sentinel; the fan pops one char.
            if (vk == VK_BACK) {
                PostMessageW(fanHwnd, FanWindow::WM_FAN_FILTER_KEY, (WPARAM)VK_BACK, 0);
                return 1;  // swallow — don't leak to the app underneath
            }

            // Enter — forward for future Phase 3 (launch top match).  Phase 0
            // fan handler ignores it, but we still swallow so the keystroke
            // doesn't hit whatever window has focus behind the fan.
            if (vk == VK_RETURN) {
                PostMessageW(fanHwnd, FanWindow::WM_FAN_FILTER_KEY, (WPARAM)VK_RETURN, 0);
                return 1;
            }

            // Printable keys — translate VK → wchar using the current keyboard
            // layout/state, then forward the character to the fan.
            //   We read the real keyboard state (shift/caps/altgr) so diacritics
            // and shifted symbols come through correctly.  ToUnicodeEx with
            // flags=0 consumes dead-key state; since we swallow the key it is
            // important we are the sole consumer (the fan never calls ToUnicode).
            BYTE kbState[256] = {};
            GetKeyboardState(kbState);
            // LL hook can fire before the async key-state table is updated, so
            // force the modifier bits from GetAsyncKeyState as a reliable source.
            // (LLKHF_SHIFT is not available under WIN32_LEAN_AND_MEAN.)
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
                kbState[VK_SHIFT] = 0x80;
            if (GetKeyState(VK_CAPITAL) & 1) kbState[VK_CAPITAL] = 0x01;
            // Ctrl/Alt suppress printable translation (shortcuts stay with the app).
            // Must use GetAsyncKeyState here too — GetKeyboardState lags behind the
            // hook, so a Ctrl+C would see 'c' without Ctrl and get swallowed.
            bool hasCtrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool hasAlt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
            if (hasCtrl || hasAlt)
                return CallNextHookEx(nullptr, nCode, wParam, lParam);

            // Only attempt translation for keys that can produce a character.
            bool isPrintableRange =
                (vk >= '0' && vk <= '9') ||          // 0x30–0x39
                (vk >= 'A' && vk <= 'Z') ||          // 0x41–0x5A
                (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) ||
                (vk >= VK_OEM_1 && vk <= VK_OEM_102) ||
                vk == VK_SPACE;
            if (!isPrintableRange)
                return CallNextHookEx(nullptr, nCode, wParam, lParam);

            wchar_t buf[8] = {};
            int n = ToUnicodeEx(vk, kb->scanCode, kbState, buf, 7, 0, nullptr);
            if (n == 1 && buf[0] >= L' ') {
                PostMessageW(fanHwnd, FanWindow::WM_FAN_FILTER_KEY, (WPARAM)buf[0], 0);
                return 1;  // swallow
            }
            // n==0 (no translation) or n<0 (dead key): let it pass through.
        }
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
        // If the foregrounded window is the taskbar (the user clicked our own
        // taskbar icon), don't close here — SC_RESTORE will toggle-close the
        // fan cleanly.  Closing here forces a redundant restore/minimize roundtrip
        // on the main window that stalls the next reopen's rendering.
        wchar_t cls[64] = {};
        GetClassNameW(hwnd, cls, 64);
        if (wcscmp(cls, L"Shell_TrayWnd") == 0 ||
            wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0) {
            return;
        }
        PostMessageW(s_instance->_hwnd, WM_MAIN_CLOSE_FAN, (WPARAM)t_hookGen, 3/*winevent*/);
    }
}

// ---------------------------------------------------------------------------
void MainWindow::ProvideIconicThumbnail(int w, int h) {
    if (w <= 0 || h <= 0) return;

    static constexpr int SrcSize = 256;
    // The hover preview shows the same state as the button: ring frame while a
    // scan runs, idle artwork otherwise.
    const WORD iconId = _iconBusy ? kTrayScanIds[_iconFrame] : IDI_APP;
    HICON hIco = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(iconId),
                                    IMAGE_ICON, SrcSize, SrcSize, LR_CREATEDIBSECTION);
    if (!hIco)
        hIco = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(iconId),
                                  IMAGE_ICON, 0, 0, LR_CREATEDIBSECTION);
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

    // Registered message, so it cannot be a case label. The taskbar button now
    // exists, which is the earliest moment SetOverlayIcon is honoured; apply the
    // current state so a scan that started before the button appeared shows up.
    if (msg == TaskbarButtonCreatedMsg()) {
        self->InitTaskbarList();
        self->ApplyOverlayIcon();
        return 0;
    }

    switch (msg) {
    case WM_SYSCOMMAND: {
        WPARAM cmd = wParam & 0xFFF0;
        if (cmd == SC_RESTORE || cmd == SC_MINIMIZE || cmd == SC_MAXIMIZE || cmd == SC_CLOSE) {
            DebugLog((L"[FanFolder] WM_SYSCOMMAND cmd=0x" + std::to_wstring(cmd) +
                L" (wParam=" + std::to_wstring(wParam) + L")\n").c_str());
        }
        if (cmd == SC_RESTORE) {
            DebugLog(L"[FanFolder] SC_RESTORE received\n");
            self->ToggleFan();
            // Intentionally DO NOT call DefWindowProc here.  Restoring the main
            // window triggers a cascade of synchronous WM_ACTIVATE /
            // WM_ACTIVATEAPP / ShowWindow(SW_MINIMIZE) messages that stall the
            // UI thread for 1-3 seconds while the FanWindow's WM_TIMER is
            // waiting to fire.  By the time it fires, the entry animation's
            // elapsed time is already past the duration, so the fan renders
            // at its final state with no animation.  The main window stays
            // minimized which is what we want.
            return 0;
        }
        // Same rationale for SC_MINIMIZE: eat it instead of round-tripping.
        if (cmd == SC_MINIMIZE) {
            DebugLog(L"[FanFolder] SC_MINIMIZE received — treating as toggle\n");
            self->ToggleFan();
            // Do NOT call DefWindowProc here: it would briefly restore the window,
            // fire WM_ACTIVATE → SW_MINIMIZE, which activates the next Z-order window
            // and causes the cursor to jump to the centre of the screen.
            PostMessageW(hwnd, WM_MAIN_SHOW_MIN, 0, 0);
            return 0;
        }
        break;
    }

    case InfoScreen::WM_INFO_DISMISSED:
        // Body clicks use zero; hook messages carry their thread generation.
        if (wParam == 0 || static_cast<int>(wParam) == self->_hookGen.load())
            self->CloseInfoScreen();
        return 0;

    case WM_MAIN_SHOW_MIN:
        ShowWindow(hwnd, SW_SHOWMINNOACTIVE);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID_VDM_RECONCILE) {
            self->ReconcileVirtualDesktop();
            return 0;
        }
        if (wParam == TIMER_ID_FOLDER_REFRESH) {
            KillTimer(hwnd, TIMER_ID_FOLDER_REFRESH);
            self->RefreshFromFolderChange();
            return 0;
        }
        if (wParam == TIMER_ID_FOLDER_VERIFY) {
            self->VerifyFolderChangedAsync();
            return 0;
        }
        if (wParam == TIMER_ID_SCAN_ANIM) {
            // Recompute first: this tick is also what notices that the prewarm
            // worker or the search walk finished, and stops the animation.
            self->RefreshBusyIndicator();
            if (self->_iconBusy) {
                self->_iconFrame = (self->_iconFrame + 1) % kScanFrames;
                self->ApplyTaskbarIcon();
                self->ApplyTrayIcon();
                self->ApplyOverlayIcon();
            }
            return 0;
        }
        break;

    case WM_POWERBROADCAST:
        // On resume from sleep the shell thumbnail service (dllhost.exe) is
        // cold for a few seconds, and any prewarm data built right before
        // suspend may contain null slots (→ blank icon squares) or stale
        // items from a now-modified folder.  Force a fresh prewarm so the
        // first post-resume fan open is fast and complete.
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            DebugLog(L"[FanFolder] WM_POWERBROADCAST resume — forcing prewarm\n");
            self->StartPrewarm(/*force*/ true);
            // A directory handle can go stale across sleep, especially on a
            // network share.  Re-open the watch instead of trusting the old one.
            self->StartFolderWatch();
        }
        return TRUE;

    case WM_DWMSENDICONICTHUMBNAIL:
        self->ProvideIconicThumbnail(HIWORD(lParam), LOWORD(lParam));
        return 0;

    case WM_ACTIVATEAPP:
        DebugLog((L"[FanFolder] WM_ACTIVATEAPP wParam=" + std::to_wstring(wParam) +
            L" fanOpen=" + std::to_wstring((int)(bool)self->_fanOpen) +
            L" tick=" + std::to_wstring(GetTickCount() - self->_fanOpenTick) + L"\n").c_str());
        if (wParam == 0 && self->_fanOpen) {
            if (self->_fanWindow && self->_fanWindow->IsDragging())
                return 0;
            DWORD now = GetTickCount();
            if (now - self->_fanOpenTick > 500)
                self->CloseFan();
            else
                DebugLog(L"[FanFolder] WM_ACTIVATEAPP: suppressed (too soon after open)\n");
        }
        return 0;

    case WM_ACTIVATE:
        DebugLog((L"[FanFolder] WM_ACTIVATE wParam=" + std::to_wstring(wParam) + L"\n").c_str());
        if (wParam != WA_INACTIVE)
            ShowWindow(hwnd, SW_SHOWMINNOACTIVE);  // SW_MINIMIZE would activate the next window → cursor jump
        return 0;

    case WM_MAIN_FOLDER_CHANGED: {
        // Queue the notification details for the prewarm worker, which patches
        // the index directory by directory. An overflowed notification buffer
        // cannot be patched, so it forces a rebuild.
        std::vector<FolderWatcher::Change> raw;
        const bool complete = self->_folderWatcher.TakeChanges(raw);
        {
            std::lock_guard<std::mutex> lk(self->_prewarmMutex);
            if (!complete) {
                self->_pendingIndexChanges.clear();
                self->_indexRebuildNeeded = true;
            } else {
                for (auto& change : raw) {
                    if (self->_pendingIndexChanges.size() >= 2048) {
                        // A mass change is cheaper to rebuild than to patch.
                        self->_pendingIndexChanges.clear();
                        self->_indexRebuildNeeded = true;
                        break;
                    }
                    self->_pendingIndexChanges.push_back(
                        { std::move(change.relPath), change.removed });
                }
            }
        }

        // Coalesce a burst into one scan. The watcher is started on Create, on a
        // folder/subfolder setting change, and after resume. If its thread dies,
        // the 60 s verifier still detects changes; do not re-open it here,
        // because CreateFileW on a dead network path would block the UI thread.
        self->_folderDirty.store(true, std::memory_order_release);
        SetTimer(hwnd, TIMER_ID_FOLDER_REFRESH, TIMER_FOLDER_REFRESH_MS, nullptr);
        return 0;
    }

    case WM_MAIN_PREWARM: {
        // Token, not pointer: an unknown token is a forged or stale message.
        auto* data = PrewarmHandoff::Take((UINT)lParam);
        std::vector<FileItem> liveItems;
        bool refreshLive = false;
        if (data) {
            {
                std::lock_guard<std::mutex> lk(self->_prewarmMutex);
                // Discard if a newer prewarm was started after this one was posted
                if (data->gen == self->_prewarmGen.load()) {
                    self->_prewarm.FreeHandles();
                    self->_prewarm = std::move(*data);
                    // Keep a copy of the item list as fallback for fast open
                    self->_cachedItems = self->_prewarm.items;
                    // An open fan keeps its old snapshot; replace it with the
                    // fresh list so a file added while the fan was visible
                    // appears without closing and reopening.
                    if (self->_fanOpen && self->_fanWindow &&
                        !self->_fanWindow->IsDragging()) {
                        liveItems = self->_prewarm.items;
                        refreshLive = true;
                    }
                } else {
                    data->FreeHandles();
                }
            }
            delete data;
        }
        if (refreshLive && self->_fanWindow)
            self->_fanWindow->ReplaceItems(std::move(liveItems));

        // If a change arrived while this scan was running, the scan may have
        // read its stamp before that change.  Schedule one more pass; a scan
        // that already covered the change yields an identical item list, and
        // ReplaceItems drops that without a repaint.
        if (self->_folderDirty.load(std::memory_order_acquire))
            SetTimer(hwnd, TIMER_ID_FOLDER_REFRESH, TIMER_FOLDER_REFRESH_MS, nullptr);
        self->RefreshBusyIndicator();
        return 0;
    }

    case WM_MAIN_CLOSE_FAN: { // WM_USER+4
        int msgGen = (int)wParam;
        int curGen = self->_hookGen.load();
        const wchar_t* src = lParam == 1 ? L"mouse" : lParam == 2 ? L"keyboard" : lParam == 3 ? L"winevent" : L"unknown";
        DebugLog((L"[FanFolder] WM_MAIN_CLOSE_FAN: fanOpen=" + std::to_wstring((int)(bool)self->_fanOpen) +
            L" msgGen=" + std::to_wstring(msgGen) + L" curGen=" + std::to_wstring(curGen) +
            L" src=" + src + L"\n").c_str());
        if (self->_fanOpen && msgGen == curGen) {
            self->_hookCloseTick = GetTickCount();
            self->CloseFan();
        }
        return 0;
    }

    // The fan closed itself after a context-menu command or an outbound drag.
    // No generation check: the post travels in-process from our own window.
    case WM_MAIN_FAN_CLOSED:
        if (self->_fanOpen)
            self->CloseFan();
        return 0;

    // A fan search walk started, moved on to another query, or finished. The
    // payload is only a hint; RefreshBusyIndicator reads the fan's real state.
    case FanWindow::WM_FAN_BUSY_STATE:
        self->RefreshBusyIndicator();
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
