// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "FanWindow.h"
#include "FileService.h"
#include "ShellDrag.h"
#include "Localization.h"

// File-scope diagnostic toggle. Set to true to re-enable the per-message /
// per-tick / slow-draw logging to %TEMP%\fanfolder_debug.log. Leave false in
// release builds — these log sites live inside 60 FPS animation ticks and
// the window proc, so the disk I/O alone can burn significant CPU.
static constexpr bool kTraceMessages = false;

// ---------------------------------------------------------------------------
// IDropTarget implementation — receives files dragged from Explorer onto the fan
// ---------------------------------------------------------------------------
class FanDropTarget : public IDropTarget {
    ULONG      _ref = 1;
    FanWindow* _fan;

    static bool HasFiles(IDataObject* pObj) {
        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        return SUCCEEDED(pObj->QueryGetData(&fmt));
    }
public:
    explicit FanDropTarget(FanWindow* fan) : _fan(fan) {}

    HRESULT QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG AddRef()  override { return ++_ref; }
    ULONG Release() override { ULONG r = --_ref; if (!r) delete this; return r; }

    HRESULT DragEnter(IDataObject* pObj, DWORD, POINTL, DWORD* pdwEffect) override {
        *pdwEffect = HasFiles(pObj) ? DROPEFFECT_MOVE : DROPEFFECT_NONE;
        _fan->OnDropHover(*pdwEffect != DROPEFFECT_NONE);
        return S_OK;
    }
    HRESULT DragOver(DWORD, POINTL, DWORD* pdwEffect) override {
        *pdwEffect = _fan->_dropHovering ? DROPEFFECT_MOVE : DROPEFFECT_NONE;
        return S_OK;
    }
    HRESULT DragLeave() override {
        _fan->OnDropHover(false);
        return S_OK;
    }
    HRESULT Drop(IDataObject* pObj, DWORD, POINTL, DWORD* pdwEffect) override {
        _fan->OnDropHover(false);
        *pdwEffect = DROPEFFECT_MOVE;
        _fan->HandleFileDrop(pObj);
        return S_OK;
    }
};

static constexpr float StartDistance      = 20.f;
static constexpr float ArcSpreadPerItem   = 1.5f;
static constexpr float MaxArcSpreadDeg    = 22.0f;
static constexpr int   FormMargin         = 20;
static constexpr int   LabelGap           = 6;
static constexpr int   BaselineItems      = 15;

int FanWindow::s_lastTaskbarAnchorX = -1;
static constexpr float HoverScaleMax      = 1.4f;
static constexpr float AnimSpeed_In       = 0.55f;
static constexpr float AnimSpeed_Out      = 0.65f;
static constexpr float EntryFadeDurationMs = 60.f;
static constexpr float ItemStageDurationMs = 14.f;
static constexpr float ItemAnimDurationMs  = 200.f;
// Fan (arc style): per-item EaseOutQuint with tight stagger + scale
static constexpr float FanStageDurationMs  = 12.f;
static constexpr float FanItemDurationMs   = 220.f;
static constexpr float FanStartScale       = 0.40f;
// Glide: per-item staggered cascade with scale + drift
static constexpr float GlideStageDurationMs = 18.f;   // per-item stagger
static constexpr float GlideItemDurationMs  = 380.f;  // each item's animation
static constexpr float GlideOffsetPx        = 42.f;   // upward drift distance
static constexpr float GlideStartScale      = 0.70f;  // start at 70% size
static constexpr float kPI = 3.14159265358979f;

static const UINT WM_ICON_BITMAP = WM_USER + 1;
static const UINT WM_ICON_ICON   = WM_USER + 2;
// WM_ICON_READY: lParam = heap-allocated IconReady* (owned by UI handler);
// wParam unused.  Used when the worker thread has pre-converted the bitmap to
// Gdiplus::Bitmap* — the UI handler just takes ownership and assigns the
// slots, avoiding the ~50ms × 15 = ~750ms HBitmapToGdiBitmap cost on the UI
// thread that otherwise stalls the animation timer on cached reopens.
static const UINT WM_ICON_READY  = WM_USER + 4;

struct IconReady {
    int              idx;
    HBITMAP          hBmp;    // may be null (then hIcon is used)
    HICON            hIcon;   // may be null
    Gdiplus::Bitmap* gdiBmp;  // pre-converted on worker thread; may be null
};
static const UINT WM_ANIM_TICK   = WM_USER + 3;

// TEMP DIAG: count per-frame conversion cost from DrawItem fallback path.
thread_local DWORD g_dbgConvertMs  = 0;
thread_local int   g_dbgConvertCnt = 0;

// ---------------------------------------------------------------------------
void FanWindow::Register(HINSTANCE hInst) {
    WNDCLASSEXW wc  = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = ClassName();
    RegisterClassExW(&wc);
}

FanWindow::FanWindow(HINSTANCE hInst, HWND hwndOwner,
                     const ConfigData& config,
                     std::vector<FileItem> items)
    : _hInst(hInst), _hwndOwner(hwndOwner),
      _config(config),
      _items(std::move(items))
{
    // Empty folder / empty recent-documents list: inject a single
    // non-interactive placeholder so the fan renders a normal window with a
    // "folder is empty" message instead of a degenerate zero-item window
    // (blank frame + perpetual spinner). This also avoids the FLT_MAX layout
    // sentinels in CalculateLayout when total == 0.
    if (_items.empty()) {
        FileItem ph;
        ph.name = EmptyFolderLabel();
        _items.push_back(std::move(ph));
        _isPlaceholder = true;
    }
}

FanWindow::~FanWindow() {
    StopAnimTimer();
    delete _labelFont;   _labelFont   = nullptr;
    delete _labelSF;     _labelSF     = nullptr;
    delete _measureSF;   _measureSF   = nullptr;
    delete _drawIA;      _drawIA      = nullptr;
    delete _shadowBmp;   _shadowBmp   = nullptr;
    FreeBackBuffer();
    if (_hwnd) {
        RevokeDragDrop(_hwnd);
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
    if (_dropTarget) { _dropTarget->Release(); _dropTarget = nullptr; }
    std::lock_guard<std::mutex> lk(_iconMutex);
    for (auto h : _bitmaps)    if (h) DeleteObject(h);
    for (auto h : _icons)      if (h) DestroyIcon(h);
    // _gdiBitmaps owns via shared_ptr — cleared automatically by vector dtor.
}

bool FanWindow::Create() {
    RebuildLabelCache();
    CalculateLayout();
    // Do NOT set hwndOwner: when the main window re-minimizes itself inside
    // WM_ACTIVATE, Win32 hides all owned popups — which would immediately
    // hide the fan.  WS_EX_TOOLWINDOW + WS_EX_TOPMOST handle z-order and
    // taskbar exclusion without needing an owner relationship.
    _hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        ClassName(), L"", WS_POPUP,
        _winX, _winY, _winWidth, _winHeight,
        nullptr, nullptr, _hInst, this);

    if (_hwnd) {
        _dropTarget = new FanDropTarget(this);
        RegisterDragDrop(_hwnd, _dropTarget);
    }
    return _hwnd != nullptr;
}

void FanWindow::AcceptPrewarmIcons(std::vector<HBITMAP>&& bitmaps,
                                    std::vector<HICON>&&   icons,
                                    std::vector<std::shared_ptr<Gdiplus::Bitmap>>&& gdiBitmaps,
                                    int                    iconSize) {
    // Free any previous (shouldn't happen, but be safe)
    for (auto h : _prewarmBitmaps) if (h) DeleteObject(h);
    for (auto h : _prewarmIcons)   if (h) DestroyIcon(h);
    _prewarmBitmaps    = std::move(bitmaps);
    _prewarmIcons      = std::move(icons);
    _prewarmGdiBitmaps = std::move(gdiBitmaps);
    _prewarmIconSize   = iconSize;
}

void FanWindow::Show() {
    if (!_hwnd) return;

    // Fresh open starts unfiltered (Phase 0: just the captured-text overlay).
    _filterText.clear();
    // Restore the visible view to the full item list. ApplyFilter rebuilds it
    // from _filterText (empty here = identity mapping).
    _visible.clear();
    _noMatchesActive = false;
    _visible.reserve(_items.size());
    for (int i = 0; i < (int)_items.size(); i++) _visible.push_back(i);

    _hasExplorerButton = (_config.folderPath != L"::GraphRecent::" && _config.folderPath != L"::RecentDocs::");
    int total = (int)_items.size() + (_hasExplorerButton ? 1 : 0);
    _itemProgress.assign(total, 0.f);
    _hoverScale.assign(total, 1.f);
    _entryProgress.assign(total, 0.f);
    _iconLoaded.assign(total, false);
    _bitmaps.assign(total, nullptr);
    _icons.assign(total, nullptr);
    _gdiBitmaps.assign(total, nullptr);  // shared_ptr; old cache released
    _entryAlpha   = 0.f;
    _animating    = true;
    if (!_drawIA) _drawIA = new Gdiplus::ImageAttributes();

    if (_config.animStyle == ConfigData::AnimStyle::None) {
        std::fill(_entryProgress.begin(), _entryProgress.end(), 1.f);
        _entryAlpha = 1.f;
    } else if (_config.animStyle == ConfigData::AnimStyle::Fade) {
        // Items at final position immediately; only alpha animates in
        std::fill(_entryProgress.begin(), _entryProgress.end(), 1.f);
    }

    // Use pre-warmed icons if the icon size matches; otherwise load async
    bool usePrewarm = (_prewarmIconSize == _iconSize) &&
                      ((int)_prewarmBitmaps.size() == (int)_items.size());

    // Arrow item doesn't need async load (only present when explorer button is shown)
    if (_hasExplorerButton) _iconLoaded[total - 1] = true;
    // Placeholder ("folder is empty") has no file to load an icon for.
    if (_isPlaceholder) {
        for (int i = 0; i < (int)_items.size(); i++) _iconLoaded[i] = true;
    } else
    for (int i = 0; i < (int)_items.size(); i++) {
        if (usePrewarm) {
            _bitmaps[i]    = _prewarmBitmaps[i];  _prewarmBitmaps[i] = nullptr;
            _icons[i]      = _prewarmIcons[i];    _prewarmIcons[i]   = nullptr;
            if (i < (int)_prewarmGdiBitmaps.size())
                _gdiBitmaps[i] = std::move(_prewarmGdiBitmaps[i]);
            // A prewarm slot can be empty if the shell thumbnailer was cold
            // (typical right after resume-from-sleep).  Kick an async load
            // instead of marking it "done" — otherwise the fan displays a
            // permanent blank square for that file.
            const bool slotHasIcon = _bitmaps[i] || _icons[i] || _gdiBitmaps[i];
            if (slotHasIcon)
                _iconLoaded[i] = true;
            else
                StartIconLoad(i);
        } else {
            StartIconLoad(i);
        }
    }
    // Release any leftover prewarm handles (size mismatch case)
    for (auto h : _prewarmBitmaps) if (h) DeleteObject(h);
    for (auto h : _prewarmIcons)   if (h) DestroyIcon(h);
    _prewarmBitmaps.clear();
    _prewarmIcons.clear();
    _prewarmGdiBitmaps.clear();
    _prewarmIconSize = 0;

    // Convert any HBITMAP/HICON that wasn't pre-converted by prewarm (e.g.
    // size-mismatch fallback path).  When prewarm supplies a cached
    // GDI+ Bitmap we skip this — it saves ~750ms on the UI thread per open.
    for (int i = 0; i < (int)_items.size(); i++) {
        if (_gdiBitmaps[i]) continue;
        HBITMAP bmp = _bitmaps[i];
        HICON   ico = _icons[i];
        if (bmp)      _gdiBitmaps[i].reset(HBitmapToGdiBitmap(bmp));
        else if (ico) _gdiBitmaps[i].reset(Gdiplus::Bitmap::FromHICON(ico));
    }

    // Phase 1: CalculateLayout() was already called in Create(), but at that
    // point _visible was still empty (it's populated above) — so the layout's
    // slot count was wrong. Recompute it now that _visible + _hasExplorerButton
    // are final. Without this, _iconPos/_hitRects are too short and every
    // item draws at (0,0), producing a stacked mess.
    CalculateLayout();

    DrawToLayeredWindow();
    ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
    // NB: Don't set _createTick here.  The animation tick posts might race
    // with other queued messages on rapid taskbar reopens.  Leave _createTick
    // at 0 and initialise it on the FIRST tick so the animation always begins
    // from t=0 when the tick is actually processed.
    _createTick = 0;

    // Kick off the animation via a threadpool timer that PostMessage()s
    // WM_ANIM_TICK.  This is higher priority than WM_TIMER (which gets
    // starved by up to ~1.5 seconds when the queue has pending messages),
    // so the entry animation starts and runs smoothly even on rapid reopens.
    StartAnimTimer();

    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, _hwnd, 0 };
    TrackMouseEvent(&tme);
}

void FanWindow::Close() {
    StopAnimTimer();
    if (_hwnd) {
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Phase 1: rebuild _visible from _items using _filterText.
// Case-insensitive substring match against ItemLabel (the display name).
// When _filterText is empty, _visible becomes the identity mapping.
// Empty match set (no filter on an empty folder) → _visible stays empty,
// and the layout/draw paths render "no matches" via the placeholder path.
// Must run on the UI thread. Re-layouts and redraws.
void FanWindow::ApplyFilter() {
    if (!_hwnd) return;

    // Lowercase copy of the filter text (case-insensitive substring match).
    std::wstring needle = _filterText;
    for (auto& c : needle) c = (wchar_t)towlower(c);

    _visible.clear();
    _noMatchesActive = false;
    if (!_filterText.empty() && !needle.empty()) {
        // Filter active: include only matching items.
        _visible.reserve(_items.size());
        for (int i = 0; i < (int)_items.size(); i++) {
            // The placeholder (empty-folder) item never matches a filter.
            if (_isPlaceholder && i == 0) continue;
            std::wstring label = (i < (int)_labelCache.size()) ? _labelCache[i]
                                                              : ItemLabel(i);
            std::wstring lower = label;
            for (auto& c : lower) c = (wchar_t)towlower(c);
            if (lower.find(needle) != std::wstring::npos)
                _visible.push_back(i);
        }
        // No matches + filter active + folder has items → no-matches placeholder
        // (an empty folder on its own is already the placeholder item, not this).
        if (_visible.empty() && !_items.empty()) _noMatchesActive = true;
    } else {
        // No filter: include all items.
        _visible.reserve(_items.size());
        for (int i = 0; i < (int)_items.size(); i++) _visible.push_back(i);
    }

    // Reset hover/drag state when the slot under the pointer is no longer visible.
    if (_hoverIdx >= TotalSlots()) _hoverIdx = -1;
    if (_dragIdx  >= TotalSlots()) _dragIdx  = -1;

    // Re-layout (arc tightens to matches) and redraw.
    CalculateLayout();
    DrawToLayeredWindow();
}

bool FanWindow::IsVisible() const {
    return _hwnd && IsWindowVisible(_hwnd);
}

void FanWindow::Reposition() {
    CalculateLayout();
    if (_hwnd)
        SetWindowPos(_hwnd, HWND_TOPMOST, _winX, _winY, _winWidth, _winHeight, SWP_NOACTIVATE);
}

// ---------------------------------------------------------------------------
// Find the taskbar window on a specific monitor.
// Checks Shell_TrayWnd (primary) and all Shell_SecondaryTrayWnd windows.
// outRect receives the taskbar's window rect on success.
HWND FanWindow::FindTaskbarOnMonitor(HMONITOR hMon, RECT& outRect) {
    // Check primary taskbar first
    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (hTray) {
        RECT r = {};
        GetWindowRect(hTray, &r);
        if (MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST) == hMon) {
            outRect = r;
            return hTray;
        }
    }

    // Search secondary taskbars
    struct Ctx { HMONITOR hMon; HWND hFound; RECT rect; };
    Ctx ctx { hMon, nullptr, {} };
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        wchar_t cls[64] = {};
        GetClassNameW(hwnd, cls, 64);
        if (wcscmp(cls, L"Shell_SecondaryTrayWnd") != 0) return TRUE;
        RECT r = {};
        GetWindowRect(hwnd, &r);
        auto* c = reinterpret_cast<Ctx*>(lp);
        if (MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST) == c->hMon) {
            c->hFound = hwnd;
            c->rect   = r;
            return FALSE;
        }
        return TRUE;
    }, (LPARAM)&ctx);
    if (ctx.hFound) { outRect = ctx.rect; return ctx.hFound; }

    // Fallback: primary taskbar rect even if on wrong monitor
    if (hTray) { GetWindowRect(hTray, &outRect); return hTray; }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Walk taskbar window tree to find the button belonging to our process.
// Handles both primary (Shell_TrayWnd) and secondary (Shell_SecondaryTrayWnd).
// Returns the button centre X (or Y for vertical taskbar) on success, -1 on failure.
int FanWindow::FindTaskbarButtonCenter(HWND hTaskbar, RECT taskbarRect) {
    if (!hTaskbar) return -1;
    DWORD ourPid = GetCurrentProcessId();

    auto findChild = [](HWND parent, const wchar_t* cls) -> HWND {
        return FindWindowExW(parent, nullptr, cls, nullptr);
    };

    // Primary taskbar: Shell_TrayWnd → ReBarWindow32 → MSTaskSwWClass → MSTaskListWClass
    // Secondary taskbar: Shell_SecondaryTrayWnd → WorkerW → MSTaskListWClass  (or direct child)
    HWND list = nullptr;
    wchar_t cls[64] = {};
    GetClassNameW(hTaskbar, cls, 64);
    if (wcscmp(cls, L"Shell_TrayWnd") == 0) {
        HWND rebar = findChild(hTaskbar, L"ReBarWindow32");
        HWND sw    = findChild(rebar ? rebar : hTaskbar, L"MSTaskSwWClass");
        list       = findChild(sw    ? sw    : hTaskbar, L"MSTaskListWClass");
    } else {
        // Secondary: try WorkerW child first, then direct child
        HWND worker = findChild(hTaskbar, L"WorkerW");
        list = findChild(worker ? worker : hTaskbar, L"MSTaskListWClass");
        if (!list) list = findChild(hTaskbar, L"MSTaskListWClass");
    }

    if (list) {
        struct FindCtx { DWORD pid; int center; };
        FindCtx ctx { ourPid, -1 };
        EnumChildWindows(list, [](HWND hwnd, LPARAM lp) -> BOOL {
            auto* ctx = reinterpret_cast<FindCtx*>(lp);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != ctx->pid) return TRUE;
            RECT r = {};
            GetWindowRect(hwnd, &r);
            ctx->center = (r.left + r.right) / 2;
            return FALSE;  // stop enumeration
        }, (LPARAM)&ctx);
        if (ctx.center >= 0) {
            s_lastTaskbarAnchorX = ctx.center;
            return ctx.center;
        }
    }

    return -1;  // walk failed — let caller use cache or fallback
}

// ---------------------------------------------------------------------------
void FanWindow::RebuildFontCache() {
    delete _labelFont;   _labelFont   = nullptr;
    delete _labelSF;     _labelSF     = nullptr;
    delete _measureSF;   _measureSF   = nullptr;

    float sz = _iconSize * 0.22f;
    _cachedFontSize = sz;
    _labelFont  = new Gdiplus::Font(L"Segoe UI", sz, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);

    _labelSF = new Gdiplus::StringFormat();
    _labelSF->SetAlignment(Gdiplus::StringAlignmentFar);
    _labelSF->SetLineAlignment(Gdiplus::StringAlignmentCenter);
    _labelSF->SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    _labelSF->SetTrimming(Gdiplus::StringTrimmingNone);

    _measureSF = new Gdiplus::StringFormat();
    _measureSF->SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
}

void FanWindow::RebuildLabelCache() {
    _labelCache.resize(_items.size());
    for (int i = 0; i < (int)_items.size(); i++)
        _labelCache[i] = ItemLabel(i);
}

// ---------------------------------------------------------------------------
void FanWindow::CalculateLayout() {
    POINT cursor = {};
    GetCursorPos(&cursor);

    // Find the monitor the cursor is on — this is where the fan should appear.
    HMONITOR hCursorMon = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hCursorMon, &mi);

    // Find the taskbar window on that specific monitor (primary or secondary).
    RECT tbRect = {};
    HWND hTaskbar = FindTaskbarOnMonitor(hCursorMon, tbRect);
    int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;

    _maxStackHeight = (int)(screenH * 0.75f);
    _iconSize = std::clamp(screenH / 19, 48, 128);

    // Measure label widths with GDI+ (reuse persistent _measureBmp to avoid allocation)
    Gdiplus::Graphics tmpG(&_measureBmp);
    if (_iconSize * 0.22f != _cachedFontSize || !_labelFont)
        RebuildFontCache();

    // Phase 1: layout is driven by visible slots (filtered items + arrow),
    // not by the full _items list. The arc tightens to matches.
    int total = TotalSlots();

    // Icon-size correction: when the arc has more than BaselineItems (15)
    // slots, the items get crowded vertically and the topmost icon overlaps
    // the one below it. Scale the icon size down so the arc height is
    // dominated by spacing, not by icon diameter. Target: icon diameter
    // < ~90% of itemSpacing * sin(arcAngle) so adjacent icons have a small
    // visual gap.
    if (total > BaselineItems) {
        // itemSpacing ≈ (_maxStackHeight - StartDistance) / (total - 1)
        // We want iconSize < itemSpacing * 0.9, so:
        float maxSpacing = (_maxStackHeight - StartDistance) / (float)(total - 1);
        int adjusted = std::clamp((int)(maxSpacing * 0.9f), 36, _iconSize);
        if (adjusted < _iconSize) {
            _iconSize = adjusted;
            if (_iconSize * 0.22f != _cachedFontSize || !_labelFont)
                RebuildFontCache();
        }
    }
    _labelWidths.resize(total);
    float maxLabelW = 0.f;

    // The "no matches" synthetic slot lives at slot 0 when _noMatchesActive is
    // set. It's rendered as a single text-only slot (no icon, no arrow).
    const std::wstring noMatchesLabel = L"no matches";

    auto measureLabelAt = [&](int slot, const std::wstring& label) {
        Gdiplus::RectF bounds;
        tmpG.MeasureString(label.c_str(), -1, _labelFont,
                           Gdiplus::PointF(0,0), _measureSF, &bounds);
        _labelWidths[slot] = bounds.Width + 20.f;
        maxLabelW = std::max(maxLabelW, _labelWidths[slot]);
    };

    // Measure labels for visible item slots.
    for (int slot = 0; slot < (int)_visible.size(); slot++) {
        int realIdx = _visible[slot];
        const std::wstring& label = (realIdx < (int)_labelCache.size())
            ? _labelCache[realIdx] : ItemLabel(realIdx);
        measureLabelAt(slot, label);
    }
    if (_noMatchesActive) {
        measureLabelAt(0, noMatchesLabel);
    }
    if (_hasExplorerButton && !_noMatchesActive) {
        Gdiplus::RectF bounds;
        tmpG.MeasureString(GetStrings().openInExplorer, -1, _labelFont,
                           Gdiplus::PointF(0,0), _measureSF, &bounds);
        _labelWidths[total - 1] = bounds.Width + 20.f;
        maxLabelW = std::max(maxLabelW, _labelWidths[total - 1]);
    }

    int tbH = tbRect.bottom - tbRect.top;
    int tbW = tbRect.right  - tbRect.left;
    bool taskbarAtBottom = tbH < tbW && tbRect.bottom >= mi.rcMonitor.bottom - 5;
    bool taskbarAtTop    = tbH < tbW && tbRect.top    <= mi.rcMonitor.top    + 5;
    bool taskbarAtLeft   = tbW < tbH && tbRect.left   <= mi.rcMonitor.left   + 5;
    // else taskbar at right

    bool cursorOnTaskbar = cursor.x >= tbRect.left && cursor.x <= tbRect.right
                        && cursor.y >= tbRect.top  && cursor.y <= tbRect.bottom;

    // Anchor strategy: always prefer the window-tree walk (FindTaskbarButtonCenter)
    // because it finds the exact button centre regardless of cursor position.
    // Cursor position is only used as a last-resort fallback for direct clicks when
    // the walk fails — the cursor must be on the taskbar for it to be meaningful.
    int anchorX, anchorY;
    if (taskbarAtBottom || taskbarAtTop) {
        int walked = FindTaskbarButtonCenter(hTaskbar, tbRect);   // updates s_lastTaskbarAnchorX on success
        if (walked >= 0) {
            anchorX = walked;
        } else if (cursorOnTaskbar) {
            anchorX = cursor.x;                         // genuine direct click, walk failed
            s_lastTaskbarAnchorX = anchorX;
        } else {
            // Only reuse the cached anchor if it lies within the current monitor's
            // taskbar — otherwise the cache is stale from a different monitor.
            bool cacheUsable = s_lastTaskbarAnchorX >= tbRect.left
                            && s_lastTaskbarAnchorX <= tbRect.right;
            anchorX = cacheUsable
                    ? s_lastTaskbarAnchorX
                    : (tbRect.left + tbRect.right) / 2;
        }
        anchorY = cursor.y;
    } else {
        // Vertical taskbar — same logic for Y axis
        int walked = FindTaskbarButtonCenter(hTaskbar, tbRect);
        if (walked >= 0) {
            anchorY = walked;
        } else if (cursorOnTaskbar) {
            anchorY = cursor.y;
            s_lastTaskbarAnchorX = anchorY;
        } else {
            bool cacheUsable = s_lastTaskbarAnchorX >= tbRect.top
                            && s_lastTaskbarAnchorX <= tbRect.bottom;
            anchorY = cacheUsable
                    ? s_lastTaskbarAnchorX
                    : (tbRect.top + tbRect.bottom) / 2;
        }
        anchorX = cursor.x;
    }

    // Arc hinge: anchor position on the taskbar edge
    float originX, originY;
    if (taskbarAtBottom)     { originX = (float)anchorX; originY = (float)tbRect.top; }
    else if (taskbarAtTop)   { originX = (float)anchorX; originY = (float)tbRect.bottom; }
    else if (taskbarAtLeft)  { originX = (float)tbRect.right; originY = (float)anchorY; }
    else                     { originX = (float)tbRect.left;  originY = (float)anchorY; }

    float halfIcon = _iconSize / 2.f;

    // Defensive: with the filter view, it's theoretically possible for the
    // visible set to be empty AND the explorer button to be hidden (virtual
    // folders). Avoid FLT_MAX sentinels in the bounding box loop below.
    if (total == 0) {
        _winWidth  = 2 * FormMargin;
        _winHeight = 2 * FormMargin;
        _iconPos.clear();
        _hitRects.clear();
        _labelWidths.clear();
        _winX = (int)tbRect.left;
        _winY = (int)tbRect.top;
        if (_hwnd) SetWindowPos(_hwnd, HWND_TOPMOST, _winX, _winY, _winWidth, _winHeight, SWP_NOACTIVATE);
        return;
    }

    // Item spacing: baseline of 15 items filling maxStackHeight — same density always
    float itemSpacing = (_maxStackHeight - StartDistance - halfIcon) / (float)(BaselineItems - 1);
    float totalNeeded = StartDistance + itemSpacing * (total - 1) + halfIcon;
    if (totalNeeded > _maxStackHeight && total > 1)
        itemSpacing = (_maxStackHeight - StartDistance - halfIcon) / (float)(total - 1);

    // Arc spread scales gently with item count
    float arcSpread = (total > 1)
        ? std::min((float)total * ArcSpreadPerItem, MaxArcSpreadDeg)
        : 0.f;

    // Polar arc: compute each item centre relative to the arc hinge
    std::vector<float> relX(total), relY(total);
    for (int i = 0; i < total; i++) {
        float t        = (total > 1) ? (float)i / (float)(total - 1) : 0.f;
        float angleDeg = 90.f - (t - 0.5f) * arcSpread;  // centred on 90°
        float angleRad = angleDeg * kPI / 180.f;
        float dist     = StartDistance + itemSpacing * i;

        if (taskbarAtBottom) {
            relX[i] =  dist * std::cos(angleRad);
            relY[i] = -dist * std::sin(angleRad);
        } else if (taskbarAtTop) {
            relX[i] =  dist * std::cos(angleRad);
            relY[i] =  dist * std::sin(angleRad);
        } else if (taskbarAtLeft) {
            relX[i] =  dist * std::sin(angleRad);
            relY[i] = -dist * std::cos(angleRad);
        } else {
            relX[i] = -dist * std::sin(angleRad);
            relY[i] = -dist * std::cos(angleRad);
        }
    }

    // Bounding box (labels extend to the left of icons)
    float extentLeft = maxLabelW + (float)LabelGap;
    float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
    for (int i = 0; i < total; i++) {
        minX = std::min(minX, relX[i] - halfIcon - extentLeft);
        minY = std::min(minY, relY[i] - halfIcon);
        maxX = std::max(maxX, relX[i] + halfIcon);
        maxY = std::max(maxY, relY[i] + halfIcon);
    }
    minX -= FormMargin; minY -= FormMargin;
    maxX += FormMargin; maxY += FormMargin;

    _winWidth  = (int)std::ceil(maxX - minX);
    _winHeight = (int)std::ceil(maxY - minY);

    // Arc hinge in form-local coordinates (used by Fan animation)
    _arcOriginX = (int)(-minX);
    _arcOriginY = (int)(-minY);

    // Icon centres and hit rects in form-local coordinates
    float offX = -minX;
    float offY = -minY;
    _iconPos.resize(total);
    _hitRects.resize(total);
    for (int i = 0; i < total; i++) {
        _iconPos[i].x = (int)(relX[i] + offX);
        _iconPos[i].y = (int)(relY[i] + offY);

        float ix = relX[i] + offX - halfIcon;
        float iy = relY[i] + offY - halfIcon;
        _hitRects[i] = {
            std::max(0, (int)(ix - extentLeft)),
            std::max(0, (int)iy),
            std::min(_winWidth,  (int)(ix + _iconSize)),
            std::min(_winHeight, (int)(iy + _iconSize))
        };
    }

    // Screen position — anchor hinge to origin, clamp to work area
    _winX = (int)(originX + minX);
    _winY = (int)(originY + minY);
    if (_winX < mi.rcWork.left)               _winX = mi.rcWork.left;
    if (_winX + _winWidth  > mi.rcWork.right) _winX = mi.rcWork.right  - _winWidth;
    if (_winY < mi.rcWork.top)                _winY = mi.rcWork.top;
    if (_winY + _winHeight > mi.rcWork.bottom)_winY = mi.rcWork.bottom - _winHeight;
}

// ---------------------------------------------------------------------------
void FanWindow::PremultiplyBitmap(Gdiplus::BitmapData& data) {
    auto* p = static_cast<BYTE*>(data.Scan0);
    for (UINT y = 0; y < data.Height; y++) {
        BYTE* px = p + y * data.Stride;
        for (UINT x = 0; x < data.Width; x++, px += 4) {
            BYTE a = px[3];
            if (a == 0) {
                px[0] = px[1] = px[2] = 0;
            } else if (a < 255) {
                px[0] = (BYTE)((px[0] * a + 128) >> 8);
                px[1] = (BYTE)((px[1] * a + 128) >> 8);
                px[2] = (BYTE)((px[2] * a + 128) >> 8);
            }
        }
    }
}

void FanWindow::InvalidateShadow() {
    delete _shadowBmp;
    _shadowBmp  = nullptr;
    _shadowIdx  = -1;
    _shadowHsc  = 0.f;
}

Gdiplus::Bitmap* FanWindow::RenderShadow(Gdiplus::Bitmap* srcBmp, float drawSz, float hsc) {
    if (!srcBmp) return nullptr;
    float hoverT = (hsc - 1.f) / (HoverScaleMax - 1.f);
    struct ShadowPass { float offset; float peakAlpha; };
    static const ShadowPass passes[] = {
        {2.f, 18.f}, {4.f, 14.f}, {6.f, 10.f}, {8.f, 6.f}, {11.f, 3.f}
    };
    float maxOff = 11.f;
    float margin = maxOff;
    int   bmpSz  = (int)(drawSz + margin * 2.f + 0.5f);
    auto* result = new Gdiplus::Bitmap(bmpSz, bmpSz, PixelFormat32bppARGB);
    Gdiplus::Graphics g(result);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    Gdiplus::ImageAttributes ia;
    for (auto& pass : passes) {
        int alpha = (int)(hoverT * pass.peakAlpha);
        if (alpha <= 0) continue;
        float shadowAlphaF = (float)alpha / 255.f;
        Gdiplus::ColorMatrix shadowCm = {
            0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,
            0,0,0, shadowAlphaF, 0,
            0,0,0,0,1
        };
        ia.SetColorMatrix(&shadowCm, Gdiplus::ColorMatrixFlagsDefault,
                          Gdiplus::ColorAdjustTypeBitmap);
        float offsets[] = {-pass.offset, 0.f, pass.offset};
        for (float ox : offsets) for (float oy : offsets) {
            if (ox == 0.f && oy == 0.f) continue;
            DrawCachedBitmapIA(g, srcBmp, margin + ox, margin + oy, drawSz, &ia);
        }
    }
    _shadowOffX = margin;
    _shadowOffY = margin;
    return result;
}

void FanWindow::FreeBackBuffer() {
    delete _backBmp;   _backBmp   = nullptr;
    if (_hdcBack)  { DeleteDC(_hdcBack);       _hdcBack   = nullptr; }
    if (_hBackDIB) { DeleteObject(_hBackDIB);  _hBackDIB  = nullptr; }
    _pBackBits = nullptr;
    _backW = _backH = 0;
}

void FanWindow::DrawToLayeredWindow() {
    if (!_hwnd || _winWidth <= 0 || _winHeight <= 0) return;

    DWORD _dbg_t0 = GetTickCount();
    DWORD _dbg_t_alloc = 0, _dbg_t_items = 0, _dbg_t_premul = 0, _dbg_t_ulw = 0;
    g_dbgConvertMs  = 0;
    g_dbgConvertCnt = 0;

    // Recreate backbuffer only when size changes
    if (_winWidth != _backW || _winHeight != _backH) {
        FreeBackBuffer();

        BITMAPINFO bi = {};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = _winWidth;
        bi.bmiHeader.biHeight      = -_winHeight;
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        HDC hdcScreen = GetDC(nullptr);
        _hBackDIB = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &_pBackBits, nullptr, 0);
        _hdcBack  = CreateCompatibleDC(hdcScreen);
        ReleaseDC(nullptr, hdcScreen);

        if (!_hBackDIB || !_hdcBack) { FreeBackBuffer(); return; }
        SelectObject(_hdcBack, _hBackDIB);

        // Wrap DIB memory directly — GDI+ renders into the DIB, no extra copy needed
        _backBmp = new Gdiplus::Bitmap(_winWidth, _winHeight, _winWidth * 4,
                                       PixelFormat32bppARGB, (BYTE*)_pBackBits);
        if (_backBmp->GetLastStatus() != Gdiplus::Ok) {
            delete _backBmp; _backBmp = nullptr;
            FreeBackBuffer();
            return;
        }
        _backW   = _winWidth;
        _backH   = _winHeight;
    }
    _dbg_t_alloc = GetTickCount() - _dbg_t0;
    DWORD _dbg_t1 = GetTickCount();

    // Clear and render into the DIB-backed GDI+ bitmap
    int _dbg_placeholders = 0, _dbg_gdiDraws = 0, _dbg_conversions = 0;
    DWORD _dbg_t_convert = 0, _dbg_t_drawImage = 0, _dbg_t_labels = 0;
    DWORD _dbg_t_gctor = 0, _dbg_t_gset = 0, _dbg_t_gclear = 0, _dbg_t_gloop = 0, _dbg_t_gdtor = 0;
    {
        // Always clear DIB to transparent first — valid baseline even when
        // GDI+ Graphics construction fails (headless/sandboxed environments).
        if (_pBackBits)
            std::memset(_pBackBits, 0, (size_t)_winWidth * _winHeight * 4);

        DWORD _gt0 = GetTickCount();
        Gdiplus::Graphics g(_backBmp);
        _dbg_t_gctor = GetTickCount() - _gt0;
        const bool gdiOk = (g.GetLastStatus() == Gdiplus::Ok);

        DWORD _gt1 = GetTickCount();
        if (gdiOk) {
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        }
        _dbg_t_gset = GetTickCount() - _gt1;
        _dbg_t_gclear = 0;
        DWORD _gt3 = GetTickCount();

        if (gdiOk) {
            int total = TotalSlots();
            // Returns alpha for a *slot*. Real-index animation vectors are
            // sized by items.size() + 1 (arrow). For the arrow slot the real
            // index would be out of range, so we look up by the arrow's
            // reserved slot (last element of the per-item vectors). For
            // real-item slots we use the mapped real index.
            auto getItemAlpha = [&](int slot) -> float {
                int realIdx = RealIndexForSlot(slot);
                bool isArrow = IsArrowSlot(slot);
                switch (_config.animStyle) {
                case ConfigData::AnimStyle::Spring: {
                    float ip;
                    if (isArrow)
                        ip = (_itemProgress.empty()) ? 1.f : _itemProgress.back();
                    else
                        ip = (realIdx < (int)_itemProgress.size()) ? _itemProgress[realIdx] : 1.f;
                    return std::clamp(ip, 0.f, 1.f) * _entryAlpha;
                }
                case ConfigData::AnimStyle::Fan:
                case ConfigData::AnimStyle::Glide: {
                    float ep;
                    if (isArrow)
                        ep = (_entryProgress.empty()) ? 1.f : _entryProgress.back();
                    else
                        ep = (realIdx < (int)_entryProgress.size()) ? _entryProgress[realIdx] : 1.f;
                    return ep;
                }
                case ConfigData::AnimStyle::Fade: {
                    float ep;
                    if (isArrow)
                        ep = (_entryProgress.empty()) ? 1.f : _entryProgress.back();
                    else
                        ep = (realIdx < (int)_entryProgress.size()) ? _entryProgress[realIdx] : 1.f;
                    return ep * _entryAlpha;
                }
                case ConfigData::AnimStyle::None:
                    return 1.f;
                }
                return 1.f;
            };

            for (int slot = 0; slot < total; slot++) {
                if (slot == _hoverIdx) continue;
                DrawItem(g, slot, getItemAlpha(slot));
            }
            if (_hoverIdx >= 0 && _hoverIdx < total)
                DrawItem(g, _hoverIdx, getItemAlpha(_hoverIdx));

            // Drop-hover: blue tinted overlay signals the fan accepts incoming files
            if (_dropHovering) {
                Gdiplus::SolidBrush overlay(Gdiplus::Color(55, 80, 160, 255));
                g.FillRectangle(&overlay, 0, 0, _winWidth, _winHeight);
            }

            // Filter-as-you-type debug overlay (Phase 0): render the captured
            // keystrokes as a pill so we can confirm the keyboard-hook → fan
            // input path works before wiring up real filtering.
            if (!_filterText.empty()) {
                std::wstring probe = L"\u2328 " + _filterText;  // keyboard glyph + text
                float pillH = _iconSize * 0.5f;
                float pillW = std::min((float)_winWidth - 16.f,
                                       32.f + (float)probe.size() * _iconSize * 0.18f);
                DrawLabelPill(g, 8.f, 8.f, pillW, pillH, pillH / 2.f, probe, 1.f);
            }
        } else {
            // GDI+ unavailable — request a redraw on the next animation tick
            // so the frame is retried once the graphics subsystem recovers.
            _iconsDirty.store(true, std::memory_order_relaxed);
        }
        _dbg_t_gloop = GetTickCount() - _gt3;
    }
    _dbg_t_gdtor = 0;
    _dbg_t_items = GetTickCount() - _dbg_t1;
    DWORD _dbg_t2 = GetTickCount();

    // Premultiply alpha in-place on the DIB bits (no LockBits/memcpy needed)
    if (_pBackBits) {
        BYTE* px = static_cast<BYTE*>(_pBackBits);
        int stride = _winWidth * 4;
        for (int y = 0; y < _winHeight; y++) {
            BYTE* row = px + y * stride;
            for (int x = 0; x < _winWidth; x++, row += 4) {
                BYTE a = row[3];
                if (a == 0) {
                    row[0] = row[1] = row[2] = 0;
                } else if (a < 255) {
                    row[0] = (BYTE)((row[0] * a + 128) >> 8);
                    row[1] = (BYTE)((row[1] * a + 128) >> 8);
                    row[2] = (BYTE)((row[2] * a + 128) >> 8);
                }
            }
        }
    }
    _dbg_t_premul = GetTickCount() - _dbg_t2;
    DWORD _dbg_t3 = GetTickCount();

    HDC hdcScreen = GetDC(nullptr);
    POINT ptSrc = {0, 0};
    SIZE  szWin = {_winWidth, _winHeight};
    POINT ptDst = {_winX, _winY};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    // Retry once on failure — the screen DC can be briefly invalid during
    // desktop switches, session locks, or RDP reconnects.
    BOOL ulwOk = UpdateLayeredWindow(_hwnd, hdcScreen, &ptDst, &szWin, _hdcBack, &ptSrc, 0, &blend, ULW_ALPHA);
    if (!ulwOk) {
        ReleaseDC(nullptr, hdcScreen);
        hdcScreen = GetDC(nullptr);
        UpdateLayeredWindow(_hwnd, hdcScreen, &ptDst, &szWin, _hdcBack, &ptSrc, 0, &blend, ULW_ALPHA);
    }
    ReleaseDC(nullptr, hdcScreen);
    _dbg_t_ulw = GetTickCount() - _dbg_t3;

    DWORD _dbg_total = GetTickCount() - _dbg_t0;
    if (kTraceMessages && _dbg_total > 50) {
        wchar_t p[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, p);
        wcscat_s(p, L"fanfolder_debug.log");
        wchar_t buf[256];
        swprintf_s(buf, L"[FanFolder] DRAW slow: alloc=%u items=%u(gctor=%u,gset=%u,gclear=%u,gloop=%u) premul=%u ulw=%u TOTAL=%ums  convert[n=%d,ms=%u]\n",
                   _dbg_t_alloc, _dbg_t_items,
                   _dbg_t_gctor, _dbg_t_gset, _dbg_t_gclear, _dbg_t_gloop,
                   _dbg_t_premul, _dbg_t_ulw, _dbg_total,
                   g_dbgConvertCnt, g_dbgConvertMs);
        HANDLE hh = CreateFileW(p, FILE_APPEND_DATA,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hh != INVALID_HANDLE_VALUE) {
            char u[512];
            int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, u, sizeof(u), nullptr, nullptr);
            if (n > 1) { DWORD w = 0; WriteFile(hh, u, n - 1, &w, nullptr); }
            CloseHandle(hh);
        }
    }
}

// ─── HBitmapToGdiBitmap ─────────────────────────────────────────────────────
// Converts an HBITMAP to a heap-allocated Gdiplus::Bitmap that owns its pixels.
// Called once per icon; result cached in _gdiBitmaps[]. Thread-safe (no DC state).
Gdiplus::Bitmap* FanWindow::HBitmapToGdiBitmap(HBITMAP hBmp) {
    if (!hBmp) return nullptr;
    BITMAP bm = {};
    if (!GetObject(hBmp, sizeof(bm), &bm) || bm.bmWidth <= 0) return nullptr;

    int w = bm.bmWidth;
    int h = std::abs(bm.bmHeight);

    // For DIB sections, read raw bits directly to preserve premultiplied alpha
    // (GetDIBits with BI_RGB zeroes the alpha channel, breaking transparent icons).
    DIBSECTION ds = {};
    bool isDib = (GetObject(hBmp, sizeof(ds), &ds) == sizeof(ds))
                 && ds.dsBm.bmBits != nullptr
                 && ds.dsBmih.biBitCount == 32;

    if (isDib) {
        bool topDown = (ds.dsBmih.biHeight < 0);
        int  stride  = ds.dsBm.bmWidthBytes;
        BYTE* src    = static_cast<BYTE*>(ds.dsBm.bmBits);

        // For top-down DIBs where stride == w*4, we can wrap memory directly
        // and avoid the intermediate vector + LockBits copy entirely.
        bool needsFlip = !topDown;
        bool hasAlpha = false;

        // Check alpha in the source pixels
        for (int i = 0; i < w * h && !hasAlpha; i++) {
            BYTE* px = src + (size_t)i * 4;
            if (px[3] != 0) hasAlpha = true;
        }

        // We still need a copy when: row order must be flipped or alpha must be patched.
        // But we create the bitmap first and LockBits into it directly (no intermediate vector).
        auto* bmpG = new Gdiplus::Bitmap(w, h, PixelFormat32bppARGB);
        if (!bmpG) return nullptr;
        Gdiplus::Rect rect(0, 0, w, h);
        Gdiplus::BitmapData bd;
        if (bmpG->LockBits(&rect, Gdiplus::ImageLockModeWrite,
                           PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
            for (int row = 0; row < h; row++) {
                int srcRow = needsFlip ? (h - 1 - row) : row;
                memcpy(static_cast<BYTE*>(bd.Scan0) + row * bd.Stride,
                       src + (size_t)srcRow * stride, (size_t)w * 4);
            }
            if (!hasAlpha) {
                for (int row = 0; row < h; row++) {
                    BYTE* px = static_cast<BYTE*>(bd.Scan0) + row * bd.Stride;
                    for (int x = 0; x < w; x++, px += 4) px[3] = 255;
                }
            }
            bmpG->UnlockBits(&bd);
        }
        return bmpG;
    }

    // Non-DIB path: use GetDIBits (rare — most shell bitmaps are DIBs)
    std::vector<BYTE> bits((size_t)w * h * 4);
    HDC     hdc  = CreateCompatibleDC(nullptr);
    HBITMAP hOld = (HBITMAP)SelectObject(hdc, hBmp);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    GetDIBits(hdc, hBmp, 0, h, bits.data(), &bi, DIB_RGB_COLORS);
    SelectObject(hdc, hOld);
    DeleteDC(hdc);
    for (int i = 0; i < w * h; i++) bits[(size_t)i * 4 + 3] = 255;

    auto* bmpG = new Gdiplus::Bitmap(w, h, PixelFormat32bppARGB);
    if (!bmpG) return nullptr;
    Gdiplus::Rect rect(0, 0, w, h);
    Gdiplus::BitmapData bd;
    if (bmpG->LockBits(&rect, Gdiplus::ImageLockModeWrite,
                       PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
        for (int row = 0; row < h; row++)
            memcpy(static_cast<BYTE*>(bd.Scan0) + row * bd.Stride,
                   bits.data() + (size_t)row * w * 4, (size_t)w * 4);
        bmpG->UnlockBits(&bd);
    }
    return bmpG;
}

// DrawCachedBitmapIA — draw a pre-cached GDI+ bitmap letterboxed into a square.
// Zero allocation per call — all conversion was done once in HBitmapToGdiBitmap.
void FanWindow::DrawCachedBitmapIA(Gdiplus::Graphics& g, Gdiplus::Bitmap* bmp,
                                   float x, float y, float size,
                                   Gdiplus::ImageAttributes* ia) {
    if (!bmp) return;
    float w = (float)bmp->GetWidth();
    float h = (float)bmp->GetHeight();
    if (w <= 0.f || h <= 0.f) return;
    float scale = std::min(size / w, size / h);
    float dstW  = w * scale, dstH = h * scale;
    float dstX  = x + (size - dstW) * 0.5f;
    float dstY  = y + (size - dstH) * 0.5f;
    Gdiplus::RectF dest(dstX, dstY, dstW, dstH);
    g.DrawImage(bmp, dest, 0, 0, w, h, Gdiplus::UnitPixel, ia);
}

void FanWindow::DrawLabelPill(Gdiplus::Graphics& g,
                               float pillLeft, float pillTop,
                               float pillW, float pillH, float radius,
                               const std::wstring& text, float alpha) {
    if (pillW <= 0 || pillH <= 0 || alpha <= 0.f) return;

    Gdiplus::GraphicsPath path;
    float d = radius * 2.f;
    path.AddArc(pillLeft,             pillTop,             d, d, 180, 90);
    path.AddArc(pillLeft + pillW - d, pillTop,             d, d, 270, 90);
    path.AddArc(pillLeft + pillW - d, pillTop + pillH - d, d, d, 0,   90);
    path.AddArc(pillLeft,             pillTop + pillH - d, d, d, 90,  90);
    path.CloseFigure();

    Gdiplus::SolidBrush fillBrush(Gdiplus::Color((BYTE)(190.f * alpha), 20, 20, 20));
    g.FillPath(&fillBrush, &path);

    if (!_labelFont) RebuildFontCache();
    Gdiplus::SolidBrush textBrush(Gdiplus::Color((BYTE)(255.f * alpha), 255, 255, 255));

    Gdiplus::RectF textRect(pillLeft + 8.f, pillTop, pillW - 16.f, pillH);
    g.DrawString(text.c_str(), -1, _labelFont, textRect, _labelSF, &textBrush);
}

void FanWindow::DrawArrowItem(Gdiplus::Graphics& g, float cx, float cy, float sz, float alpha) {
    float r    = sz * 0.18f;
    float left = cx - sz / 2.f;
    float top  = cy - sz / 2.f;
    float d    = r * 2.f;

    Gdiplus::GraphicsPath bg;
    bg.AddArc(left,          top,          d, d, 180, 90);
    bg.AddArc(left + sz - d, top,          d, d, 270, 90);
    bg.AddArc(left + sz - d, top + sz - d, d, d, 0,   90);
    bg.AddArc(left,          top + sz - d, d, d, 90,  90);
    bg.CloseFigure();

    Gdiplus::SolidBrush bgBrush(Gdiplus::Color((BYTE)(220.f * alpha), 60, 68, 92));
    g.FillPath(&bgBrush, &bg);

    float arm = sz * 0.22f;
    Gdiplus::Pen pen(Gdiplus::Color((BYTE)(255.f * alpha), 255, 255, 255), sz * 0.08f);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
    g.DrawLine(&pen, cx - arm * 0.5f, cy - arm, cx + arm * 0.5f, cy);
    g.DrawLine(&pen, cx + arm * 0.5f, cy,        cx - arm * 0.5f, cy + arm);
}

// Returns the display name for item idx, respecting the ShowExtensions setting.
// Directories always show their name as-is.
std::wstring FanWindow::ItemLabel(int idx) const {
    if (idx < 0 || idx >= (int)_items.size()) return {};
    const auto& item = _items[idx];
    if (_isPlaceholder) return item.name;  // "folder is empty" — show verbatim
    if (item.isDirectory) return item.name;

    // For .lnk shortcuts strip the .lnk suffix first so the real filename is shown
    std::wstring displayName = item.name;
    if (!item.targetPath.empty()) {
        auto dot = displayName.rfind(L'.');
        if (dot != std::wstring::npos) {
            std::wstring ext = displayName.substr(dot);
            for (auto& c : ext) c = (wchar_t)towlower(c);
            if (ext == L".lnk") displayName = displayName.substr(0, dot);
        }
    }

    if (_config.showExtensions) {
        if (displayName.size() > 42)
            displayName = displayName.substr(0, 40) + L"\u2026";
        return displayName;
    }
    auto dot = displayName.rfind(L'.');
    if (dot == std::wstring::npos || dot == 0) {
        if (displayName.size() > 42)
            displayName = displayName.substr(0, 40) + L"\u2026";
        return displayName;
    }
    displayName = displayName.substr(0, dot);
    if (displayName.size() > 42)
        displayName = displayName.substr(0, 40) + L"\u2026";
    return displayName;
}

void FanWindow::DrawItem(Gdiplus::Graphics& g, int slot, float itemAlpha) {
    if (itemAlpha <= 0.f) return;
    if (slot < 0 || slot >= TotalSlots()) return;

    // Phase 1: the drawing loop iterates over *slots*.  Map slot→real index,
    // and treat the arrow slot (last slot, only if shown) as a special case.
    int realIdx = RealIndexForSlot(slot);
    bool isArrow = IsArrowSlot(slot);

    // "no matches" synthetic slot — text only, no icon, no arrow.
    if (_noMatchesActive) {
        int total = TotalSlots();
        // _labelWidths and _iconPos for slot 0 were set by CalculateLayout.
        // Compute the centre the same way the polar arc did.
        float cx = (slot < (int)_iconPos.size()) ? (float)_iconPos[slot].x : 0.f;
        float cy = (slot < (int)_iconPos.size()) ? (float)_iconPos[slot].y : 0.f;
        float drawSz = _iconSize;
        float pillW = (slot < (int)_labelWidths.size()) ? _labelWidths[slot] : 100.f;
        float pillH = drawSz * 0.45f;
        if (pillH < 20.f) pillH = 20.f;
        float pillLeft = cx - pillW / 2.f;
        float pillTop  = cy - pillH / 2.f;
        DrawLabelPill(g, pillLeft, pillTop, pillW, pillH, pillH / 2.f,
                      L"no matches", itemAlpha);
        return;
    }

    float hsc    = (realIdx < (int)_hoverScale.size()) ? _hoverScale[realIdx] : 1.f;
    float cx     = (slot < (int)_iconPos.size()) ? (float)_iconPos[slot].x : 0.f;
    float cy     = (slot < (int)_iconPos.size()) ? (float)_iconPos[slot].y : 0.f;
    float drawSz = (float)_iconSize * hsc;
    float entryP = (realIdx < (int)_entryProgress.size()) ? _entryProgress[realIdx] : 1.f;

    switch (_config.animStyle) {
    case ConfigData::AnimStyle::Spring: {
        float ip = (realIdx < (int)_itemProgress.size()) ? _itemProgress[realIdx] : 1.f;
        float scale = std::max(ip, 0.01f);
        drawSz = _iconSize * scale * hsc;
        break;
    }
    case ConfigData::AnimStyle::Fan:
        // Position flies from arc origin; scale grows from FanStartScale to 1.0
        cx = _arcOriginX + entryP * (cx - _arcOriginX);
        cy = _arcOriginY + entryP * (cy - _arcOriginY);
        drawSz = _iconSize * (FanStartScale + (1.f - FanStartScale) * entryP) * hsc;
        break;
    case ConfigData::AnimStyle::Glide:
        // Drift up GlideOffsetPx; subtle scale from GlideStartScale to 1.0
        cy += GlideOffsetPx * (1.f - entryP);
        drawSz = _iconSize * (GlideStartScale + (1.f - GlideStartScale) * entryP) * hsc;
        break;
    case ConfigData::AnimStyle::None:
    case ConfigData::AnimStyle::Fade:
        break;
    }

    if (isArrow) {
        DrawArrowItem(g, cx, cy, drawSz, itemAlpha);
        float pillH = drawSz * 0.45f;
        if (pillH < 20.f) pillH = 20.f;
        float pillW = (slot < (int)_labelWidths.size()) ? _labelWidths[slot] : 100.f;
        float pillLeft = cx - drawSz / 2.f - LabelGap - pillW;
        float pillTop  = cy - pillH / 2.f;
        DrawLabelPill(g, pillLeft, pillTop, pillW, pillH, pillH / 2.f,
                      GetStrings().openInExplorer, itemAlpha);
        return;
    }

    // File icon
    float iconX = cx - drawSz / 2.f;
    float iconY = cy - drawSz / 2.f;

    HBITMAP bmp = nullptr;
    HICON   ico  = nullptr;
    {
        std::lock_guard<std::mutex> lk(_iconMutex);
        if (realIdx < (int)_bitmaps.size()) bmp = _bitmaps[realIdx];
        if (realIdx < (int)_icons.size())   ico = _icons[realIdx];
    }

    // Lazy-cache: convert HBITMAP/HICON → Gdiplus::Bitmap* once, reuse every frame.
    // This eliminates the ~65KB heap allocation that DrawShellBitmapIA did per frame.
    if (realIdx < (int)_gdiBitmaps.size() && !_gdiBitmaps[realIdx]) {
        DWORD _cvt0 = GetTickCount();
        if (bmp)      _gdiBitmaps[realIdx].reset(HBitmapToGdiBitmap(bmp));
        else if (ico) _gdiBitmaps[realIdx].reset(Gdiplus::Bitmap::FromHICON(ico));
        DWORD _cvt = GetTickCount() - _cvt0;
        g_dbgConvertMs  += _cvt;
        g_dbgConvertCnt += 1;
    }
    Gdiplus::Bitmap* gdiBmp = (realIdx < (int)_gdiBitmaps.size()) ? _gdiBitmaps[realIdx].get() : nullptr;

    bool hoverActive = (realIdx < (int)_hoverScale.size() && _hoverScale[realIdx] > 1.01f);
    if (!isArrow && hoverActive && gdiBmp != nullptr) {
        // Rebuild shadow bitmap only when hover target or scale changes significantly
        float quantHsc = floorf(hsc * 20.f + 0.5f) / 20.f;  // quantize to avoid thrashing
        if (_shadowIdx != realIdx || _shadowHsc != quantHsc) {
            delete _shadowBmp;
            _shadowBmp  = RenderShadow(gdiBmp, drawSz, hsc);
            _shadowIdx  = realIdx;
            _shadowHsc  = quantHsc;
        }
        if (_shadowBmp) {
            g.DrawImage(_shadowBmp,
                        iconX - _shadowOffX, iconY - _shadowOffY,
                        (float)_shadowBmp->GetWidth(),
                        (float)_shadowBmp->GetHeight());
        }
    }

    if (gdiBmp) {
        Gdiplus::ColorMatrix cm = {
            1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0,
            0,0,0,itemAlpha,0, 0,0,0,0,1
        };
        _drawIA->SetColorMatrix(&cm, Gdiplus::ColorMatrixFlagsDefault,
                                Gdiplus::ColorAdjustTypeBitmap);
        DrawCachedBitmapIA(g, gdiBmp, iconX, iconY, drawSz, _drawIA);
    } else if (!_isPlaceholder) {
        // Placeholder while icon loads (the empty-folder message item has no
        // icon at all — draw just its label pill, no grey box).
        Gdiplus::SolidBrush ph(Gdiplus::Color((BYTE)(80.f * itemAlpha), 150, 150, 150));
        g.FillRectangle(&ph, Gdiplus::RectF(iconX, iconY, drawSz, drawSz));
    }

    // Label pill
    const std::wstring& name = (realIdx < (int)_labelCache.size())
        ? _labelCache[realIdx] : _items[realIdx].name;
    float pillH = drawSz * 0.45f;
    if (pillH < 20.f) pillH = 20.f;
    float pillW    = (slot < (int)_labelWidths.size()) ? _labelWidths[slot] : 100.f;
    float pillLeft = cx - drawSz / 2.f - LabelGap - pillW;
    float pillTop  = cy - pillH / 2.f;
    DrawLabelPill(g, pillLeft, pillTop, pillW, pillH, pillH / 2.f, name, itemAlpha);
}

// ---------------------------------------------------------------------------
int FanWindow::HitTest(int x, int y) const {
    // The empty-folder placeholder is non-interactive: no hover, click, drag,
    // or context menu. Reporting no hit everywhere keeps it purely informational.
    if (_isPlaceholder) return -1;
    for (int i = (int)_hitRects.size() - 1; i >= 0; i--) {
        if (x >= _hitRects[i].left  && x < _hitRects[i].right &&
            y >= _hitRects[i].top   && y < _hitRects[i].bottom)
            return i;
    }
    return -1;
}

void FanWindow::LaunchItem(int slot) {
    // Phase 1: caller passes a SLOT. Map to a real index for the item launch
    // path; the arrow slot opens Explorer.
    if (IsArrowSlot(slot)) {
        // "Open in Explorer" — for RecentDocs sentinel, open the Recent folder
        const std::wstring& fp = _config.folderPath;
        if (!fp.empty()) {
            if (fp == L"::RecentDocs::" || fp == L"::RecentFiles::" || fp == L"::GraphRecent::") {
                wchar_t recentPath[MAX_PATH] = {};
                if (SHGetFolderPathW(nullptr, CSIDL_RECENT, nullptr, SHGFP_TYPE_CURRENT, recentPath) == S_OK)
                    ShellExecuteW(nullptr, L"open", L"explorer.exe", recentPath, nullptr, SW_SHOWNORMAL);
            } else {
                ShellExecuteW(nullptr, L"open", L"explorer.exe", fp.c_str(), nullptr, SW_SHOWNORMAL);
            }
        }
        Close();
        return;
    }

    int realIdx = RealIndexForSlot(slot);
    if (realIdx >= 0 && realIdx < (int)_items.size()) {
        const std::wstring& path = _items[realIdx].fullPath;

        // For online Office documents (SharePoint/OneDrive), use the Office
        // URI protocol handlers so the file opens in the desktop app, not the browser.
        bool launched = false;
        if (path.size() > 8 &&
            (_wcsnicmp(path.c_str(), L"https://", 8) == 0 ||
             _wcsnicmp(path.c_str(), L"http://",  7) == 0))
        {
            // Determine protocol from file extension stored in targetPath / name
            const std::wstring& extSource = _items[realIdx].targetPath.empty()
                                          ? _items[realIdx].name
                                          : _items[realIdx].targetPath;
            const wchar_t* dot = PathFindExtensionW(extSource.c_str());
            const wchar_t* proto = nullptr;
            if (dot && *dot) {
                if      (_wcsicmp(dot, L".docx") == 0 || _wcsicmp(dot, L".doc")  == 0 ||
                         _wcsicmp(dot, L".docm") == 0 || _wcsicmp(dot, L".dotx") == 0 ||
                         _wcsicmp(dot, L".dotm") == 0 || _wcsicmp(dot, L".odt")  == 0)
                    proto = L"ms-word";
                else if (_wcsicmp(dot, L".xlsx") == 0 || _wcsicmp(dot, L".xls")  == 0 ||
                         _wcsicmp(dot, L".xlsm") == 0 || _wcsicmp(dot, L".xlsb") == 0 ||
                         _wcsicmp(dot, L".xltx") == 0 || _wcsicmp(dot, L".ods")  == 0)
                    proto = L"ms-excel";
                else if (_wcsicmp(dot, L".pptx") == 0 || _wcsicmp(dot, L".ppt")  == 0 ||
                         _wcsicmp(dot, L".pptm") == 0 || _wcsicmp(dot, L".potx") == 0 ||
                         _wcsicmp(dot, L".ppsx") == 0 || _wcsicmp(dot, L".odp")  == 0)
                    proto = L"ms-powerpoint";
            }
            if (proto) {
                // ms-word:ofe|u|https://... opens the document for editing in the desktop app
                std::wstring uri = std::wstring(proto) + L":ofe|u|" + path;
                HINSTANCE hr = ShellExecuteW(nullptr, L"open", uri.c_str(),
                                             nullptr, nullptr, SW_SHOWNORMAL);
                launched = (reinterpret_cast<INT_PTR>(hr) > 32);
            }
        }

        if (!launched)
            ShellExecuteW(nullptr, L"open", path.c_str(),
                          nullptr, nullptr, SW_SHOWNORMAL);
    }
    Close();
}

// ---------------------------------------------------------------------------
void FanWindow::OnDropHover(bool hovering) {
    if (_dropHovering == hovering) return;
    _dropHovering = hovering;
    DrawToLayeredWindow();
}

void FanWindow::HandleFileDrop(IDataObject* pDataObj) {
    // Drop is not supported in virtual folder modes
    if (_config.folderPath == L"::RecentDocs::" || _config.folderPath == L"::RecentFiles::" || _config.folderPath == L"::GraphRecent::") return;

    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};
    if (FAILED(pDataObj->GetData(&fmt, &stg))) return;

    HDROP hDrop = static_cast<HDROP>(stg.hGlobal);
    UINT  count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

    bool anyCopied = false;
    for (UINT i = 0; i < count; i++) {
        wchar_t srcPath[MAX_PATH + 2] = {};
        if (!DragQueryFileW(hDrop, i, srcPath, MAX_PATH)) continue;

        const wchar_t* name = PathFindFileNameW(srcPath);
        std::wstring dstPath = _config.folderPath + L"\\" + name;

        // Double-null terminated strings required by SHFILEOPSTRUCTW
        wchar_t srcBuf[MAX_PATH + 2] = {};  wcscpy_s(srcBuf, srcPath);
        wchar_t dstBuf[MAX_PATH + 2] = {};  wcscpy_s(dstBuf, dstPath.c_str());

        SHFILEOPSTRUCTW op = {};
        op.wFunc  = FO_MOVE;
        op.pFrom  = srcBuf;
        op.pTo    = dstBuf;
        op.fFlags = FOF_RENAMEONCOLLISION | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
        if (SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted)
            anyCopied = true;
    }
    ReleaseStgMedium(&stg);

    if (anyCopied) {
        // Reload items and icons so the fan reflects the new file immediately
        _items = FileService::ScanFolder(_config.folderPath, _config.maxItems,
                                         _config.includeDirs, _config.filterRegex,
                                         _config.sortMode);
        _hasExplorerButton = (_config.folderPath != L"::GraphRecent::" && _config.folderPath != L"::RecentDocs::");
        // A drop only happens in a real (non-virtual) folder and after a
        // successful copy, so _items now has real content — clear any prior
        // empty-folder placeholder state.
        _isPlaceholder = false;
        if (_items.empty()) {
            FileItem ph; ph.name = EmptyFolderLabel();
            _items.push_back(std::move(ph));
            _isPlaceholder = true;
        }
        int total = (int)_items.size() + (_hasExplorerButton ? 1 : 0);

        // Reset icon arrays to the new size; start async loads for all items
        {
            std::lock_guard<std::mutex> lk(_iconMutex);
            for (auto h : _bitmaps)    if (h) DeleteObject(h);
            for (auto h : _icons)      if (h) DestroyIcon(h);
            _bitmaps.assign(total, nullptr);
            _icons.assign(total, nullptr);
            _iconLoaded.assign(total, false);
            _gdiBitmaps.assign(total, nullptr);  // shared_ptr — releases old cache
        }
        _iconSize = _prewarmIconSize > 0 ? _prewarmIconSize : 64;

        // Phase 1: reset the visible view — _items was just replaced, so the
        // old _visible indices are stale. Start unfiltered (ApplyFilter with
        // empty _filterText = identity mapping).
        _filterText.clear();
        _visible.clear();
        _noMatchesActive = false;
        _visible.reserve(_items.size());
        for (int i = 0; i < (int)_items.size(); i++) _visible.push_back(i);

        RebuildLabelCache();
        CalculateLayout();
        if (!_isPlaceholder)
            for (int i = 0; i < (int)_items.size(); i++)
                StartIconLoad(i);
        else if (!_iconLoaded.empty())
            _iconLoaded[0] = true;
        DrawToLayeredWindow();
    }
}

void FanWindow::ShowContextMenu(int slot, POINT screenPt) {
    // Phase 1: caller passes a SLOT.  Map to a real index for the item path.
    // The arrow slot has no context menu — open the folder's context menu.
    if (IsArrowSlot(slot)) {
        ShellExecuteW(nullptr, L"open", L"explorer.exe",
                      _config.folderPath.c_str(), nullptr, SW_SHOWNORMAL);
        PostMessageW(_hwndOwner, WM_USER + 4, 0, 0);  // close fan
        return;
    }
    int realIdx = RealIndexForSlot(slot);
    const std::wstring& path = (realIdx >= 0 && realIdx < (int)_items.size())
        ? _items[realIdx].fullPath : _config.folderPath;
    if (path.empty()) return;

    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(path.c_str());
    if (!pidl) return;

    IShellFolder* psf = nullptr;
    PCUITEMID_CHILD pidlChild = nullptr;
    HRESULT hr = SHBindToParent(pidl, IID_PPV_ARGS(&psf), &pidlChild);
    if (FAILED(hr)) { ILFree(pidl); return; }

    IContextMenu* pcm = nullptr;
    PCUITEMID_CHILD apidl[] = { pidlChild };
    hr = psf->GetUIObjectOf(_hwnd, 1, apidl, IID_IContextMenu, nullptr, (void**)&pcm);
    psf->Release();
    ILFree(pidl);
    if (FAILED(hr) || !pcm) return;

    IContextMenu2* pcm2 = nullptr;
    pcm->QueryInterface(IID_PPV_ARGS(&pcm2));

    HMENU hMenu = CreatePopupMenu();
    pcm->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL | CMF_EXPLORE);

    int cmd = (int)TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                    screenPt.x, screenPt.y, _hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd > 0) {
        CMINVOKECOMMANDINFOEX cmi = {};
        cmi.cbSize  = sizeof(cmi);
        cmi.fMask   = CMIC_MASK_UNICODE;
        cmi.hwnd    = _hwnd;
        cmi.lpVerb  = MAKEINTRESOURCEA(cmd - 1);
        cmi.lpVerbW = MAKEINTRESOURCEW(cmd - 1);
        cmi.nShow   = SW_SHOWNORMAL;
        pcm->InvokeCommand(reinterpret_cast<CMINVOKECOMMANDINFO*>(&cmi));
    }

    if (pcm2) pcm2->Release();
    pcm->Release();

    PostMessageW(_hwndOwner, WM_USER + 4, 0, 0);
}

void FanWindow::StartIconLoad(int idx) {
    if (idx < 0 || idx >= (int)_items.size()) return;
    HWND      hwnd = _hwnd;
    std::wstring p  = _items[idx].fullPath;
    std::wstring tp = _items[idx].targetPath; // may be empty if fast-scan was used
    int       sz   = _iconSize;

    struct IconWork { HWND hwnd; int idx; std::wstring p, tp; int sz; };
    auto* work = new IconWork{hwnd, idx, std::move(p), std::move(tp), sz};

    TrySubmitThreadpoolCallback([](PTP_CALLBACK_INSTANCE, PVOID ctx) {
        auto* w = static_cast<IconWork*>(ctx);
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        // Resolve .lnk target lazily if prewarm fast-scan skipped it
        if (w->tp.empty()) {
            auto dot = w->p.rfind(L'.');
            if (dot != std::wstring::npos) {
                std::wstring ext = w->p.substr(dot);
                for (auto& c : ext) c = (wchar_t)towlower(c);
                if (ext == L".lnk") {
                    bool isDir = false;
                    std::wstring resolved = FileService::ResolveLnk(w->p, isDir);
                    if (!resolved.empty() && !isDir)
                        w->tp = resolved;
                }
            }
        }

        // Use resolved target for content thumbnails, fall back to .lnk path for shell icon
        const std::wstring& contentPath = w->tp.empty() ? w->p : w->tp;
        HBITMAP bmp = nullptr;

        // Detect online items (SharePoint / OneDrive) — p is a URL, tp is the filename
        const bool isOnline = (w->p.size() > 8 &&
                               (_wcsnicmp(w->p.c_str(), L"https://", 8) == 0 ||
                                _wcsnicmp(w->p.c_str(), L"http://",  7) == 0));

        // NOTE: FileService's GDI+-heavy functions (GetImageThumbnail,
        // GetShellBitmapByExtension) self-serialise via an internal mutex to
        // avoid GDI+ lock contention with the UI thread's Graphics calls.
        if (!isOnline) {
            if (FileService::IsSvgExtension(contentPath))
                bmp = FileService::GetSvgThumbnail(contentPath, w->sz);

            if (!bmp && FileService::IsGdiImageExtension(contentPath))
                bmp = FileService::GetImageThumbnail(contentPath, w->sz);

            if (!bmp && FileService::IsShellThumbnailExtension(contentPath))
                bmp = FileService::GetShellThumbnail(contentPath, w->sz);

            if (!bmp)
                bmp = FileService::GetShellBitmap(w->p, w->sz);  // shell resolves .lnk for icon
        }

        // Post raw HBITMAP/HICON to UI thread.  Do NOT call GDI+
        // (HBitmapToGdiBitmap / Bitmap::FromHICON) here — GDI+ has a global
        // lock and concurrent worker conversions stall the UI thread's
        // Graphics::Clear / DrawImage calls for hundreds of milliseconds.
        auto postReady = [&](HBITMAP hBmp, HICON hIco) {
            auto* r = new IconReady{ w->idx, hBmp, hIco, nullptr };
            // If the fan HWND has been destroyed between when this worker was
            // submitted and now, PostMessage returns FALSE without queuing —
            // the IconReady allocation and its HBITMAP/HICON would leak.
            // Free them explicitly on that path.
            if (!PostMessageW(w->hwnd, WM_ICON_READY, 0, (LPARAM)r)) {
                if (r->hBmp)  DeleteObject(r->hBmp);
                if (r->hIcon) DestroyIcon(r->hIcon);
                delete r;
            }
        };

        if (bmp) {
            postReady(bmp, nullptr);
        } else {
            // For online items use the filename/extension for type icon lookup;
            // for local items fall back to the full path.
            std::wstring iconPath;
            if (isOnline) {
                // tp should be the filename (e.g. "document.docx") — guard against
                // tp accidentally containing a URL by checking for "://"
                if (!w->tp.empty() && w->tp.find(L"://") == std::wstring::npos)
                    iconPath = w->tp;
                else if (!w->tp.empty()) {
                    auto sl = w->tp.rfind(L'/');
                    auto qs = w->tp.find(L'?');
                    iconPath = (sl != std::wstring::npos)
                        ? w->tp.substr(sl + 1, qs == std::wstring::npos ? std::wstring::npos : qs - sl - 1)
                        : w->tp;
                }
            } else {
                iconPath = w->p;
            }
            if (isOnline) {
                HBITMAP iconBmp = FileService::GetShellBitmapByExtension(iconPath, w->sz);
                postReady(iconBmp, nullptr);
            } else {
                HICON ico = FileService::GetShellIcon(iconPath);
                if (ico)
                    postReady(nullptr, ico);
                else
                    postReady(nullptr, nullptr);
            }
        }
        CoUninitialize();
        delete w;
    }, work, nullptr);
}

// ---------------------------------------------------------------------------
// Animation tick driver.
//
// Uses a Windows threadpool timer (PTP_TIMER) which runs on a worker thread
// and PostMessage()s WM_ANIM_TICK to the fan window at ~60 FPS.  This replaces
// the original SetTimer/WM_TIMER approach because WM_TIMER is a low-priority
// message that GetMessage()/PeekMessage() only generate when the thread's
// queue is otherwise empty.  On rapid taskbar reopens, queued input/paint
// messages can starve WM_TIMER for up to ~1.5 seconds — long enough that the
// user sees the fan appear fully-formed with no animation.
// PostMessage()d WM_USER messages have normal priority and are delivered
// immediately, so the entry animation starts on time every reopen.
// ---------------------------------------------------------------------------
VOID CALLBACK FanWindow::AnimTimerCb(PTP_CALLBACK_INSTANCE, PVOID ctx, PTP_TIMER) {
    HWND hwnd = (HWND)ctx;
    // IsWindow is safe across threads — avoids posting to a destroyed HWND
    // in the small window between Close() and ~FanWindow().
    if (IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_ANIM_TICK, 0, 0);
    }
}

void FanWindow::StartAnimTimer() {
    if (_animTimer) return;
    _animTimer = CreateThreadpoolTimer(AnimTimerCb, (PVOID)_hwnd, nullptr);
    if (_animTimer) {
        // Fire immediately, then every 16 ms (~60 FPS).
        FILETIME ft = {};  // due-time 0 means "as soon as possible"
        SetThreadpoolTimer(_animTimer, &ft, 16, 0);
    }
}

void FanWindow::StopAnimTimer() {
    if (!_animTimer) return;
    // Cancel any pending fires, then wait for any in-flight callback to
    // complete.  Must happen BEFORE the HWND is destroyed so AnimTimerCb
    // never races against DestroyWindow.
    SetThreadpoolTimer(_animTimer, nullptr, 0, 0);
    WaitForThreadpoolTimerCallbacks(_animTimer, TRUE);
    CloseThreadpoolTimer(_animTimer);
    _animTimer = nullptr;
}

// ---------------------------------------------------------------------------
FanWindow* FanWindow::FromHWND(HWND hwnd) {
    return reinterpret_cast<FanWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK FanWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    FanWindow* self = FromHWND(hwnd);
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    // TEMP: trace every message + processing time during the first 1500 ms
    // after window creation, to diagnose UI-thread stalls. Gated by the
    // file-scope kTraceMessages flag; disabled in release builds.
    struct TraceGuard {
        HWND  hwnd; UINT msg; WPARAM wp; DWORD t0; bool active;
        TraceGuard(HWND h, UINT m, WPARAM w, DWORD ct)
            : hwnd(h), msg(m), wp(w), t0(GetTickCount()), active(false) {
            if constexpr (!kTraceMessages) return;
            // If _createTick is 0 we haven't had our first tick yet (still in
            // the critical window); if >0 but within 1500 ms we're still in
            // the diagnostic window.
            if (ct != 0 && (t0 - ct) > 1500) return;
            active = true;
        }
        ~TraceGuard() {
            if (!active) return;
            DWORD dt = GetTickCount() - t0;
            // Log every message during the diagnostic window so we can
            // identify stalls by gaps between entries.
            wchar_t p[MAX_PATH] = {};
            GetTempPathW(MAX_PATH, p);
            wcscat_s(p, L"fanfolder_debug.log");
            wchar_t buf[256];
            swprintf_s(buf, L"[FanFolder]   msg 0x%04X wp=0x%X t=%u dt=%ums\n",
                       msg, (unsigned)wp, t0, dt);
            HANDLE h = CreateFileW(p, FILE_APPEND_DATA,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                char u[512];
                int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, u, sizeof(u), nullptr, nullptr);
                if (n > 1) { DWORD w = 0; WriteFile(h, u, n - 1, &w, nullptr); }
                CloseHandle(h);
            }
        }
    } _trace(hwnd, msg, wParam, self->_createTick);

    switch (msg) {
    // ── Animation tick (posted by threadpool timer; see StartAnimTimer) ────
    case WM_ANIM_TICK: {
        DWORD now     = GetTickCount();
        if (self->_createTick == 0) {
            self->_createTick = now;
            if constexpr (kTraceMessages) {
                // One-off log: measure threadpool-timer → UI latency.
                wchar_t logPath[MAX_PATH] = {};
                GetTempPathW(MAX_PATH, logPath);
                wcscat_s(logPath, L"fanfolder_debug.log");
                wchar_t buf[256];
                swprintf_s(buf, L"[FanFolder] FanWindow: first WM_ANIM_TICK at tick=%u\n", now);
                HANDLE h = CreateFileW(logPath, FILE_APPEND_DATA,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (h != INVALID_HANDLE_VALUE) {
                    char utf8[512];
                    int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, utf8, sizeof(utf8), nullptr, nullptr);
                    if (n > 1) { DWORD w = 0; WriteFile(h, utf8, n - 1, &w, nullptr); }
                    CloseHandle(h);
                }
            }
        }
        float elapsed = (float)(now - self->_createTick);

        if constexpr (kTraceMessages) {
            // TEMP diagnostic: log first 5 ticks per open to see elapsed progression
            static thread_local int s_tickCount = 0;
            static thread_local HWND s_tickHwnd = nullptr;
            if (s_tickHwnd != hwnd) { s_tickHwnd = hwnd; s_tickCount = 0; }
            if (s_tickCount < 5) {
                wchar_t logPath2[MAX_PATH] = {};
                GetTempPathW(MAX_PATH, logPath2);
                wcscat_s(logPath2, L"fanfolder_debug.log");
                wchar_t buf2[256];
                swprintf_s(buf2, L"[FanFolder]   tick #%d elapsed=%.0fms entryAlpha=%.2f itemProg[0]=%.2f\n",
                           s_tickCount, elapsed, self->_entryAlpha,
                           self->_itemProgress.empty() ? 0.f : self->_itemProgress[0]);
                HANDLE h2 = CreateFileW(logPath2, FILE_APPEND_DATA,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (h2 != INVALID_HANDLE_VALUE) {
                    char utf8b[512];
                    int n = WideCharToMultiByte(CP_UTF8, 0, buf2, -1, utf8b, sizeof(utf8b), nullptr, nullptr);
                    if (n > 1) { DWORD w = 0; WriteFile(h2, utf8b, n - 1, &w, nullptr); }
                    CloseHandle(h2);
                }
                s_tickCount++;
            }
        }

        bool  dirty   = false;

            // Phase 1: iterate over *slots* (visible items + arrow). The arrow
            // slot's animation lives at the last index of the per-item vectors
            // (which are sized by items.size() + 1).
            int total = self->TotalSlots();
            int arrowRealIdx = self->_hasExplorerButton
                ? (int)self->_items.size() : -1;

            // Build a list of real indices to animate (one per visible slot).
            // For real-item slots, that's _visible[slot]; for the arrow slot,
            // it's the arrow's reserved index at items.size().
            std::vector<int> realIndices;
            realIndices.reserve(total);
            for (int slot = 0; slot < total; slot++) {
                if (self->IsArrowSlot(slot) && arrowRealIdx >= 0)
                    realIndices.push_back(arrowRealIdx);
                else
                    realIndices.push_back(self->RealIndexForSlot(slot));
            }

            // The animation's stagger order is by SLOT position, so that
            // filtering the list naturally re-orders the cascade. We use the
            // slot's position in the realIndices array as the stagger index.
            auto slotIdxOf = [&](int real) -> int {
                for (int k = 0; k < (int)realIndices.size(); k++)
                    if (realIndices[k] == real) return k;
                return 0;
            };

            switch (self->_config.animStyle) {
            case ConfigData::AnimStyle::Spring: {
                float newAlpha = std::min(elapsed / EntryFadeDurationMs, 1.f);
                if (newAlpha != self->_entryAlpha) { self->_entryAlpha = newAlpha; dirty = true; }
                for (int real : realIndices) {
                    if (real < 0 || real >= (int)self->_itemProgress.size()) continue;
                    int s = slotIdxOf(real);
                    float stagger = s * ItemStageDurationMs;
                    float t = std::clamp((elapsed - stagger) / ItemAnimDurationMs, 0.f, 1.f);
                    float u = 1.f - t;
                    float easedT = 1.f - u * u * u;
                    float prog = easedT + sinf(t * kPI) * 0.12f;
                    if (prog != self->_itemProgress[real]) { self->_itemProgress[real] = prog; dirty = true; }
                }
                break;
            }
            case ConfigData::AnimStyle::Fan: {
                // Per-item: position + scale + alpha all tied to entryProgress
                // _entryAlpha unused for Fan (set to 1 so getItemAlpha works)
                self->_entryAlpha = 1.f;
                for (int real : realIndices) {
                    if (real < 0 || real >= (int)self->_entryProgress.size()) continue;
                    int s = slotIdxOf(real);
                    float stagger = s * FanStageDurationMs;
                    float t = std::clamp((elapsed - stagger) / FanItemDurationMs, 0.f, 1.f);
                    float u = 1.f - t;
                    float eased = 1.f - u * u * u * u * u;  // EaseOutQuint
                    if (eased != self->_entryProgress[real]) { self->_entryProgress[real] = eased; dirty = true; }
                }
                break;
            }
            case ConfigData::AnimStyle::Glide: {
                // Per-item stagger: items cascade in from bottom, each with its own
                // EaseOutQuart curve. Alpha is per-item (tied to progress), no
                // separate window fade needed.
                self->_entryAlpha = 1.f;
                for (int real : realIndices) {
                    if (real < 0 || real >= (int)self->_entryProgress.size()) continue;
                    int s = slotIdxOf(real);
                    float stagger = s * GlideStageDurationMs;
                    float t = std::clamp((elapsed - stagger) / GlideItemDurationMs, 0.f, 1.f);
                    float u = 1.f - t;
                    float eased = 1.f - u * u * u * u;  // EaseOutQuart
                    if (eased != self->_entryProgress[real]) { self->_entryProgress[real] = eased; dirty = true; }
                }
                break;
            }
            case ConfigData::AnimStyle::None:
                break;
            case ConfigData::AnimStyle::Fade: {
                float newAlpha = std::min(elapsed / 150.f, 1.f);
                if (newAlpha != self->_entryAlpha) { self->_entryAlpha = newAlpha; dirty = true; }
                break;
            }
            }

            // Phase 1: hover scale is keyed by slot. _hoverIdx is a slot.
            for (int slot = 0; slot < total; slot++) {
                int real = realIndices[slot];
                if (real < 0 || real >= (int)self->_hoverScale.size()) continue;
                float target = (slot == self->_hoverIdx) ? HoverScaleMax : 1.f;
                float speed  = (slot == self->_hoverIdx) ? AnimSpeed_In  : AnimSpeed_Out;
                float ns     = self->_hoverScale[real] + speed * (target - self->_hoverScale[real]);
                ns = std::clamp(ns, 1.f, HoverScaleMax);
                if (std::abs(ns - self->_hoverScale[real]) > 0.001f) {
                    self->_hoverScale[real] = ns; dirty = true;
                }
            }

            // Coalesced icon-load invalidation (set by WM_USER + 1/+2 handlers)
            if (self->_iconsDirty.exchange(false, std::memory_order_acq_rel))
                dirty = true;

            if (dirty) self->DrawToLayeredWindow();

            // ── Drag detection (timer-based polling) ───────────────────────
            // WS_EX_NOACTIVATE windows can never be foreground, so SetCapture
            // does not route mouse-moves outside the window. Poll instead.
            if (self->_dragIdx >= 0 && !self->_dragging) {
                if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                    POINT pt;
                    GetCursorPos(&pt);
                    int dx = pt.x - self->_dragStart.x;
                    int dy = pt.y - self->_dragStart.y;
                    if (dx * dx + dy * dy > 25) {
                        self->_dragging = true;
                        int slot = self->_dragIdx;
                        self->_dragIdx = -1;
                        // Phase 1: _dragIdx is a slot. Map to real index.
                        int realIdx = self->RealIndexForSlot(slot);
                        if (realIdx >= 0 && realIdx < (int)self->_items.size()) {
                            HBITMAP bmp = (realIdx < (int)self->_bitmaps.size()) ? self->_bitmaps[realIdx] : nullptr;
                            DoShellDrag(hwnd, self->_items[realIdx].fullPath, bmp, self->_iconSize);
                        }
                        self->_dragging = false;
                        PostMessageW(self->_hwndOwner, WM_USER + 4, 0, 0);
                    }
                } else {
                    // Button released without reaching drag threshold — cancel
                    self->_dragIdx = -1;
                }
            }
        }
        return 0;

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        int hit = self->HitTest(x, y);
        if (hit != self->_hoverIdx) {
            self->_hoverIdx = hit;
            SetCursor(LoadCursor(nullptr, hit >= 0 ? IDC_HAND : IDC_ARROW));
        }

        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        self->_hoverIdx = -1;
        return 0;

    case WM_LBUTTONDOWN: {
        int hit = self->HitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (hit >= 0) {
            self->_dragIdx  = hit;
            self->_dragging = false;
            GetCursorPos(&self->_dragStart);  // screen coords — matches GetCursorPos in timer
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        if (!self->_dragging) {
            int hit = self->HitTest(x, y);
            if (hit >= 0 && hit == self->_dragIdx)
                self->LaunchItem(hit);
        }
        self->_dragIdx  = -1;
        self->_dragging = false;
        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int hit = self->HitTest(pt.x, pt.y);
        ClientToScreen(hwnd, &pt);
        if (hit >= 0)
            self->ShowContextMenu(hit, pt);
        return 0;
    }

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);
        return (self->HitTest(pt.x, pt.y) >= 0) ? HTCLIENT : HTTRANSPARENT;
    }

    // ── Async icon results ─────────────────────────────────────────────────
    // Worker thread returns only the raw HBITMAP/HICON (it must NOT call
    // GDI+ because GDI+ has a global lock; concurrent worker conversions
    // would block the UI thread's Graphics calls for ~700 ms on every
    // cached reopen).  We do the cheap GDI+ conversion here on the UI
    // thread — contention-free, it takes ~1 ms per icon.
    case WM_ICON_READY: {
        std::unique_ptr<IconReady> r(reinterpret_cast<IconReady*>(lParam));
        if (!r) return 0;
        int idx = r->idx;
        {
            std::lock_guard<std::mutex> lk(self->_iconMutex);
            if (idx < (int)self->_bitmaps.size()) {
                if (self->_bitmaps[idx]) DeleteObject(self->_bitmaps[idx]);
                self->_bitmaps[idx] = r->hBmp;
            }
            if (idx < (int)self->_icons.size()) {
                if (self->_icons[idx]) DestroyIcon(self->_icons[idx]);
                self->_icons[idx] = r->hIcon;
            }
            if (idx < (int)self->_iconLoaded.size())
                self->_iconLoaded[idx] = true;
        }
        if (idx < (int)self->_gdiBitmaps.size()) {
            Gdiplus::Bitmap* bmp = nullptr;
            if (r->hBmp)       bmp = FanWindow::HBitmapToGdiBitmap(r->hBmp);
            else if (r->hIcon) bmp = Gdiplus::Bitmap::FromHICON(r->hIcon);
            self->_gdiBitmaps[idx].reset(bmp);
        }
        self->_iconsDirty.store(true, std::memory_order_release);
        return 0;
    }

    // ── Filter-as-you-type key capture (Phase 0/1) ───────────────────────
    // Posted by MainWindow's LL keyboard hook while the fan is open. The hook
    // has already translated the keystroke to a character (layout/modifier
    // aware), so wParam is either the VK_BACK sentinel or a literal wchar_t to
    // append.  Phase 1 wires this to ApplyFilter() so each keystroke
    // re-filters the visible view and tightens the arc to matches.
    case WM_FAN_FILTER_KEY: {
        wchar_t ch = (wchar_t)wParam;
        if (ch == VK_BACK) {
            if (!self->_filterText.empty()) {
                self->_filterText.pop_back();
                self->ApplyFilter();
            }
        } else if (ch == VK_RETURN) {
            // Phase 3 will launch the top match here.  Phase 1: no-op.
        } else if (ch >= L' ') {
            self->_filterText.push_back(ch);
            self->ApplyFilter();
        }
        return 0;
    }

    case WM_DESTROY:
        // Animation timer is stopped by Close()/~FanWindow() before
        // DestroyWindow is called, so nothing to do here.
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
