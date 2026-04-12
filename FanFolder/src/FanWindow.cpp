#include "pch.h"
#include "FanWindow.h"
#include "FileService.h"
#include "ShellDrag.h"

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
static constexpr float kPI = 3.14159265358979f;

static const UINT WM_ICON_BITMAP = WM_USER + 1;
static const UINT WM_ICON_ICON   = WM_USER + 2;

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
{}

FanWindow::~FanWindow() {
    delete _labelFont;   _labelFont   = nullptr;
    delete _labelSF;     _labelSF     = nullptr;
    delete _measureSF;   _measureSF   = nullptr;
    delete _drawIA;      _drawIA      = nullptr;
    FreeBackBuffer();
    if (_hwnd) {
        RevokeDragDrop(_hwnd);
        KillTimer(_hwnd, 1);
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
    if (_dropTarget) { _dropTarget->Release(); _dropTarget = nullptr; }
    std::lock_guard<std::mutex> lk(_iconMutex);
    for (auto h : _bitmaps)    if (h) DeleteObject(h);
    for (auto h : _icons)      if (h) DestroyIcon(h);
    for (auto p : _gdiBitmaps) delete p;
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
                                    int                    iconSize) {
    // Free any previous (shouldn't happen, but be safe)
    for (auto h : _prewarmBitmaps) if (h) DeleteObject(h);
    for (auto h : _prewarmIcons)   if (h) DestroyIcon(h);
    _prewarmBitmaps  = std::move(bitmaps);
    _prewarmIcons    = std::move(icons);
    _prewarmIconSize = iconSize;
}

void FanWindow::Show() {
    if (!_hwnd) return;

    int total = (int)_items.size() + 1;
    _itemProgress.assign(total, 0.f);
    _hoverScale.assign(total, 1.f);
    _entryProgress.assign(total, 0.f);
    _iconLoaded.assign(total, false);
    _bitmaps.assign(total, nullptr);
    _icons.assign(total, nullptr);
    for (auto p : _gdiBitmaps) delete p;
    _gdiBitmaps.assign(total, nullptr);
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

    // Arrow item doesn't need async load
    _iconLoaded[total - 1] = true;
    for (int i = 0; i < (int)_items.size(); i++) {
        if (usePrewarm) {
            _bitmaps[i]    = _prewarmBitmaps[i];  _prewarmBitmaps[i] = nullptr;
            _icons[i]      = _prewarmIcons[i];    _prewarmIcons[i]   = nullptr;
            _iconLoaded[i] = true;
        } else {
            StartIconLoad(i);
        }
    }
    // Release any leftover prewarm handles (size mismatch case)
    for (auto h : _prewarmBitmaps) if (h) DeleteObject(h);
    for (auto h : _prewarmIcons)   if (h) DestroyIcon(h);
    _prewarmBitmaps.clear();
    _prewarmIcons.clear();
    _prewarmIconSize = 0;

    // Eagerly convert all available HBITMAP/HICON → Gdiplus::Bitmap* now,
    // so the first DrawToLayeredWindow() frame has zero conversion cost and
    // the animation timer starts with a clean slate.
    for (int i = 0; i < (int)_items.size(); i++) {
        if (_gdiBitmaps[i]) continue;
        HBITMAP bmp = _bitmaps[i];
        HICON   ico = _icons[i];
        if (bmp)      _gdiBitmaps[i] = HBitmapToGdiBitmap(bmp);
        else if (ico) _gdiBitmaps[i] = Gdiplus::Bitmap::FromHICON(ico);
    }

    DrawToLayeredWindow();
    ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
    _createTick = GetTickCount();   // start clock after window is visible
    SetTimer(_hwnd, 1, 16, nullptr);

    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, _hwnd, 0 };
    TrackMouseEvent(&tme);
}

void FanWindow::Close() {
    if (_hwnd) {
        KillTimer(_hwnd, 1);
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
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
// Walk Shell_TrayWnd → ReBarWindow32 → MSTaskSwWClass → MSTaskListWClass,
// enumerate child windows to find the button belonging to our process.
// Returns the button centre X on success, -1 on failure.
int FanWindow::FindTaskbarButtonCenter(RECT taskbarRect) {
    DWORD ourPid = GetCurrentProcessId();

    auto findChild = [](HWND parent, const wchar_t* cls) -> HWND {
        return FindWindowExW(parent, nullptr, cls, nullptr);
    };

    HWND tray  = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (tray) {
        HWND rebar = findChild(tray,  L"ReBarWindow32");
        HWND sw    = findChild(rebar ? rebar : tray, L"MSTaskSwWClass");
        HWND list  = findChild(sw    ? sw    : tray, L"MSTaskListWClass");
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

    // GetWindowRect(Shell_TrayWnd) returns coordinates in the calling process's
    // DPI context (per-monitor logical pixels), consistent with GetCursorPos and
    // GetMonitorInfoW.  SHAppBarMessage returns physical/system-DPI coordinates
    // which mismatch on monitors with non-100% DPI scaling — do NOT use it.
    RECT tbRect = {};
    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (hTray)
        GetWindowRect(hTray, &tbRect);

    HMONITOR hMon = MonitorFromRect(&tbRect, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);
    int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;

    _maxStackHeight = (int)(screenH * 0.75f);
    _iconSize = std::clamp(screenH / 19, 48, 128);

    // Measure label widths with GDI+
    Gdiplus::Bitmap tmpBmp(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics tmpG(&tmpBmp);
    if (_iconSize * 0.22f != _cachedFontSize || !_labelFont)
        RebuildFontCache();

    int total = (int)_items.size() + 1;
    _labelWidths.resize(total);
    float maxLabelW = 0.f;

    for (int i = 0; i < (int)_items.size(); i++) {
        Gdiplus::RectF bounds;
        const std::wstring& label = (i < (int)_labelCache.size()) ? _labelCache[i] : ItemLabel(i);
        tmpG.MeasureString(label.c_str(), -1, _labelFont,
                           Gdiplus::PointF(0,0), _measureSF, &bounds);
        _labelWidths[i] = bounds.Width + 20.f;
        maxLabelW = std::max(maxLabelW, _labelWidths[i]);
    }
    {
        Gdiplus::RectF bounds;
        tmpG.MeasureString(L"Open in Explorer", -1, _labelFont,
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
        int walked = FindTaskbarButtonCenter(tbRect);   // updates s_lastTaskbarAnchorX on success
        if (walked >= 0) {
            anchorX = walked;
        } else if (cursorOnTaskbar) {
            anchorX = cursor.x;                         // genuine direct click, walk failed
            s_lastTaskbarAnchorX = anchorX;
        } else {
            anchorX = s_lastTaskbarAnchorX >= 0
                    ? s_lastTaskbarAnchorX
                    : (tbRect.left + tbRect.right) / 2;
        }
        anchorY = cursor.y;
    } else {
        // Vertical taskbar — same logic for Y axis
        int walked = FindTaskbarButtonCenter(tbRect);
        if (walked >= 0) {
            anchorY = walked;
        } else if (cursorOnTaskbar) {
            anchorY = cursor.y;
            s_lastTaskbarAnchorX = anchorY;
        } else {
            anchorY = s_lastTaskbarAnchorX >= 0
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
                px[0] = (BYTE)((px[0] * a) >> 8);
                px[1] = (BYTE)((px[1] * a) >> 8);
                px[2] = (BYTE)((px[2] * a) >> 8);
            }
        }
    }
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

        _backBmp = new Gdiplus::Bitmap(_winWidth, _winHeight, PixelFormat32bppARGB);
        _backW   = _winWidth;
        _backH   = _winHeight;
    }

    // Clear and render into the cached GDI+ bitmap
    {
        Gdiplus::Graphics g(_backBmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));

        int total = (int)_items.size() + 1;
        auto getItemAlpha = [&](int i) -> float {
            switch (_config.animStyle) {
            case ConfigData::AnimStyle::Spring: {
                float ip = (i < (int)_itemProgress.size()) ? _itemProgress[i] : 0.f;
                return std::clamp(ip, 0.f, 1.f) * _entryAlpha;
            }
            case ConfigData::AnimStyle::Fan:
            case ConfigData::AnimStyle::Glide:
            case ConfigData::AnimStyle::Fade:
                return (i < (int)_entryProgress.size()) ? _entryProgress[i] * _entryAlpha : 0.f;
            case ConfigData::AnimStyle::None:
                return 1.f;
            }
            return 1.f;
        };

        for (int i = 0; i < total; i++) {
            if (i == _hoverIdx) continue;
            DrawItem(g, i, getItemAlpha(i));
        }
        if (_hoverIdx >= 0 && _hoverIdx < total)
            DrawItem(g, _hoverIdx, getItemAlpha(_hoverIdx));

        // Drop-hover: blue tinted overlay signals the fan accepts incoming files
        if (_dropHovering) {
            Gdiplus::SolidBrush overlay(Gdiplus::Color(55, 80, 160, 255));
            g.FillRectangle(&overlay, 0, 0, _winWidth, _winHeight);
        }
    }

    Gdiplus::Rect rect(0, 0, _winWidth, _winHeight);
    Gdiplus::BitmapData bd;
    if (_backBmp->LockBits(&rect,
                           Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite,
                           PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
        PremultiplyBitmap(bd);
        auto* src = static_cast<BYTE*>(bd.Scan0);
        auto* dst = static_cast<BYTE*>(_pBackBits);
        if ((int)bd.Stride == _winWidth * 4) {
            memcpy(dst, src, (size_t)_winHeight * _winWidth * 4);
        } else {
            for (int y = 0; y < _winHeight; y++)
                memcpy(dst + (size_t)y * _winWidth * 4,
                       src + (size_t)y * bd.Stride,
                       (size_t)_winWidth * 4);
        }
        _backBmp->UnlockBits(&bd);
    }

    HDC hdcScreen = GetDC(nullptr);
    POINT ptSrc = {0, 0};
    SIZE  szWin = {_winWidth, _winHeight};
    POINT ptDst = {_winX, _winY};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(_hwnd, hdcScreen, &ptDst, &szWin, _hdcBack, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, hdcScreen);
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
    std::vector<BYTE> bits((size_t)w * h * 4);

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
        for (int row = 0; row < h; row++) {
            int srcRow = topDown ? row : (h - 1 - row);
            memcpy(bits.data() + (size_t)row * w * 4,
                   src + (size_t)srcRow * stride, (size_t)w * 4);
        }
        bool hasAlpha = false;
        for (int i = 0; i < w * h && !hasAlpha; i++)
            if (bits[(size_t)i * 4 + 3] != 0) hasAlpha = true;
        if (!hasAlpha)
            for (int i = 0; i < w * h; i++) bits[(size_t)i * 4 + 3] = 255;
    } else {
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
    }

    // Create an owning Gdiplus::Bitmap (LockBits copies pixels into GDI+ memory)
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
    if (_config.showExtensions || item.isDirectory) return item.name;
    auto dot = item.name.rfind(L'.');
    if (dot == std::wstring::npos || dot == 0) return item.name;
    return item.name.substr(0, dot);
}

void FanWindow::DrawItem(Gdiplus::Graphics& g, int idx, float itemAlpha) {
    if (itemAlpha <= 0.f) return;
    if (idx >= (int)_iconPos.size()) return;

    float hsc    = (idx < (int)_hoverScale.size()) ? _hoverScale[idx] : 1.f;
    float cx     = (float)_iconPos[idx].x;
    float cy     = (float)_iconPos[idx].y;
    float drawSz = (float)_iconSize * hsc;
    float entryP = (idx < (int)_entryProgress.size()) ? _entryProgress[idx] : 1.f;

    switch (_config.animStyle) {
    case ConfigData::AnimStyle::Spring: {
        float ip = (idx < (int)_itemProgress.size()) ? _itemProgress[idx] : 1.f;
        float scale = std::max(ip, 0.01f);
        drawSz = _iconSize * scale * hsc;
        break;
    }
    case ConfigData::AnimStyle::Fan:
        cx = _arcOriginX + entryP * (cx - _arcOriginX);
        cy = _arcOriginY + entryP * (cy - _arcOriginY);
        break;
    case ConfigData::AnimStyle::Glide:
        cy += 32.f * (1.f - entryP);
        break;
    case ConfigData::AnimStyle::None:
    case ConfigData::AnimStyle::Fade:
        break;
    }

    int total    = (int)_items.size() + 1;
    bool isArrow = (idx == total - 1);
    if (isArrow) {
        DrawArrowItem(g, cx, cy, drawSz, itemAlpha);
        float pillH = drawSz * 0.45f;
        if (pillH < 20.f) pillH = 20.f;
        float pillW = (idx < (int)_labelWidths.size()) ? _labelWidths[idx] : 100.f;
        float pillLeft = cx - drawSz / 2.f - LabelGap - pillW;
        float pillTop  = cy - pillH / 2.f;
        DrawLabelPill(g, pillLeft, pillTop, pillW, pillH, pillH / 2.f,
                      L"Open in Explorer", itemAlpha);
        return;
    }

    // File icon
    float iconX = cx - drawSz / 2.f;
    float iconY = cy - drawSz / 2.f;

    HBITMAP bmp = nullptr;
    HICON   ico  = nullptr;
    {
        std::lock_guard<std::mutex> lk(_iconMutex);
        if (idx < (int)_bitmaps.size()) bmp = _bitmaps[idx];
        if (idx < (int)_icons.size())   ico = _icons[idx];
    }

    // Lazy-cache: convert HBITMAP/HICON → Gdiplus::Bitmap* once, reuse every frame.
    // This eliminates the ~65KB heap allocation that DrawShellBitmapIA did per frame.
    if (idx < (int)_gdiBitmaps.size() && !_gdiBitmaps[idx]) {
        if (bmp)      _gdiBitmaps[idx] = HBitmapToGdiBitmap(bmp);
        else if (ico) _gdiBitmaps[idx] = Gdiplus::Bitmap::FromHICON(ico);
    }
    Gdiplus::Bitmap* gdiBmp = (idx < (int)_gdiBitmaps.size()) ? _gdiBitmaps[idx] : nullptr;

    bool hoverActive = (idx < (int)_hoverScale.size() && _hoverScale[idx] > 1.01f);
    if (!isArrow && hoverActive && gdiBmp != nullptr) {
        float hoverT = (hsc - 1.f) / (HoverScaleMax - 1.f);
        struct ShadowPass { float offset; float peakAlpha; };
        static const ShadowPass passes[] = {
            {2.f, 18.f}, {4.f, 14.f}, {6.f, 10.f}, {8.f, 6.f}, {11.f, 3.f}
        };
        for (auto& pass : passes) {
            int alpha = (int)(hoverT * pass.peakAlpha);
            if (alpha <= 0) continue;
            float shadowAlphaF = (float)alpha / 255.f;
            Gdiplus::ColorMatrix shadowCm = {
                0,0,0,0,0,  0,0,0,0,0,  0,0,0,0,0,
                0,0,0, shadowAlphaF, 0,
                0,0,0,0,1
            };
            _drawIA->SetColorMatrix(&shadowCm, Gdiplus::ColorMatrixFlagsDefault,
                                    Gdiplus::ColorAdjustTypeBitmap);
            float offsets[] = {-pass.offset, 0.f, pass.offset};
            for (float ox : offsets) for (float oy : offsets) {
                if (ox == 0.f && oy == 0.f) continue;
                DrawCachedBitmapIA(g, gdiBmp, iconX + ox, iconY + oy, drawSz, _drawIA);
            }
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
    } else {
        // Placeholder while icon loads
        Gdiplus::SolidBrush ph(Gdiplus::Color((BYTE)(80.f * itemAlpha), 150, 150, 150));
        g.FillRectangle(&ph, Gdiplus::RectF(iconX, iconY, drawSz, drawSz));
    }

    // Label pill
    const std::wstring& name = (idx < (int)_labelCache.size())
        ? _labelCache[idx] : _items[idx].name;
    float pillH = drawSz * 0.45f;
    if (pillH < 20.f) pillH = 20.f;
    float pillW    = (idx < (int)_labelWidths.size()) ? _labelWidths[idx] : 100.f;
    float pillLeft = cx - drawSz / 2.f - LabelGap - pillW;
    float pillTop  = cy - pillH / 2.f;
    DrawLabelPill(g, pillLeft, pillTop, pillW, pillH, pillH / 2.f, name, itemAlpha);
}

// ---------------------------------------------------------------------------
int FanWindow::HitTest(int x, int y) const {
    for (int i = (int)_hitRects.size() - 1; i >= 0; i--) {
        if (x >= _hitRects[i].left  && x < _hitRects[i].right &&
            y >= _hitRects[i].top   && y < _hitRects[i].bottom)
            return i;
    }
    return -1;
}

void FanWindow::LaunchItem(int idx) {
    int total = (int)_items.size() + 1;
    if (idx == total - 1) {
        if (!_config.folderPath.empty())
            ShellExecuteW(nullptr, L"open", L"explorer.exe",
                          _config.folderPath.c_str(), nullptr, SW_SHOWNORMAL);
    } else if (idx >= 0 && idx < (int)_items.size()) {
        ShellExecuteW(nullptr, L"open", _items[idx].fullPath.c_str(),
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
        int total = (int)_items.size() + 1;

        // Reset icon arrays to the new size; start async loads for all items
        {
            std::lock_guard<std::mutex> lk(_iconMutex);
            for (auto h : _bitmaps)    if (h) DeleteObject(h);
            for (auto h : _icons)      if (h) DestroyIcon(h);
            for (auto p : _gdiBitmaps) delete p;
            _bitmaps.assign(total, nullptr);
            _icons.assign(total, nullptr);
            _iconLoaded.assign(total, false);
            _gdiBitmaps.assign(total, nullptr);
        }
        _iconSize = _prewarmIconSize > 0 ? _prewarmIconSize : 64;

        RebuildLabelCache();
        CalculateLayout();
        for (int i = 0; i < (int)_items.size(); i++)
            StartIconLoad(i);
        DrawToLayeredWindow();
    }
}

void FanWindow::ShowContextMenu(int idx, POINT screenPt) {
    const std::wstring& path = (idx >= 0 && idx < (int)_items.size())
        ? _items[idx].fullPath : _config.folderPath;
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
    int       sz   = _iconSize;

    std::thread([hwnd, idx, p, sz]() {
        HBITMAP bmp = nullptr;

        if (FileService::IsSvgExtension(p))
            bmp = FileService::GetSvgThumbnail(p, sz);     // lunasvg direct render

        if (!bmp && FileService::IsGdiImageExtension(p))
            bmp = FileService::GetImageThumbnail(p, sz);   // GDI+ direct — actual content

        if (!bmp && FileService::IsShellThumbnailExtension(p))
            bmp = FileService::GetShellThumbnail(p, sz);   // shell thumbnail (webp)

        if (!bmp)
            bmp = FileService::GetShellBitmap(p, sz);      // SIIGBF_ICONONLY (pdf, etc.)

        if (bmp) {
            PostMessageW(hwnd, WM_ICON_BITMAP, (WPARAM)idx, (LPARAM)bmp);
        } else {
            HICON ico = FileService::GetShellIcon(p);
            if (ico)
                PostMessageW(hwnd, WM_ICON_ICON, (WPARAM)idx, (LPARAM)ico);
            else
                PostMessageW(hwnd, WM_ICON_BITMAP, (WPARAM)idx, (LPARAM)nullptr);
        }
    }).detach();
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

    switch (msg) {
    // ── Animation tick ─────────────────────────────────────────────────────
    case WM_TIMER:
        if (wParam == 1) {
            DWORD now     = GetTickCount();
            float elapsed = (float)(now - self->_createTick);
            bool  dirty   = false;

            int total = (int)self->_items.size() + 1;

            switch (self->_config.animStyle) {
            case ConfigData::AnimStyle::Spring: {
                float newAlpha = std::min(elapsed / EntryFadeDurationMs, 1.f);
                if (newAlpha != self->_entryAlpha) { self->_entryAlpha = newAlpha; dirty = true; }
                for (int i = 0; i < total && i < (int)self->_itemProgress.size(); i++) {
                    float stagger = i * ItemStageDurationMs;
                    float t = std::clamp((elapsed - stagger) / ItemAnimDurationMs, 0.f, 1.f);
                    float u = 1.f - t;
                    float easedT = 1.f - u * u * u;
                    float prog = easedT + sinf(t * kPI) * 0.12f;
                    if (prog != self->_itemProgress[i]) { self->_itemProgress[i] = prog; dirty = true; }
                }
                break;
            }
            case ConfigData::AnimStyle::Fan: {
                float newAlpha = std::min(elapsed / 80.f, 1.f);
                if (newAlpha != self->_entryAlpha) { self->_entryAlpha = newAlpha; dirty = true; }
                for (int i = 0; i < total && i < (int)self->_entryProgress.size(); i++) {
                    float stagger = i * 15.f;
                    float t = std::clamp((elapsed - stagger) / 165.f, 0.f, 1.f);
                    float u = 1.f - t;
                    float eased = 1.f - u * u * u * u;
                    if (eased != self->_entryProgress[i]) { self->_entryProgress[i] = eased; dirty = true; }
                }
                break;
            }
            case ConfigData::AnimStyle::Glide: {
                float newAlpha = std::min(elapsed / 60.f, 1.f);
                if (newAlpha != self->_entryAlpha) { self->_entryAlpha = newAlpha; dirty = true; }
                float t = std::clamp(elapsed / 250.f, 0.f, 1.f);
                float u = 1.f - t;
                float eased = 1.f - u * u * u;
                for (int i = 0; i < total && i < (int)self->_entryProgress.size(); i++) {
                    if (eased != self->_entryProgress[i]) { self->_entryProgress[i] = eased; dirty = true; }
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

            for (int i = 0; i < total && i < (int)self->_hoverScale.size(); i++) {
                float target = (i == self->_hoverIdx) ? HoverScaleMax : 1.f;
                float speed  = (i == self->_hoverIdx) ? AnimSpeed_In  : AnimSpeed_Out;
                float ns     = self->_hoverScale[i] + speed * (target - self->_hoverScale[i]);
                ns = std::clamp(ns, 1.f, HoverScaleMax);
                if (std::abs(ns - self->_hoverScale[i]) > 0.001f) {
                    self->_hoverScale[i] = ns; dirty = true;
                }
            }

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
                        int idx = self->_dragIdx;
                        self->_dragIdx = -1;
                        if (idx < (int)self->_items.size()) {
                            HBITMAP bmp = (idx < (int)self->_bitmaps.size()) ? self->_bitmaps[idx] : nullptr;
                            DoShellDrag(hwnd, self->_items[idx].fullPath, bmp, self->_iconSize);
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
    case WM_USER + 1: { // bitmap
        int     idx = (int)wParam;
        HBITMAP bmp = (HBITMAP)lParam;
        {
            std::lock_guard<std::mutex> lk(self->_iconMutex);
            if (idx < (int)self->_bitmaps.size()) {
                if (self->_bitmaps[idx]) DeleteObject(self->_bitmaps[idx]);
                self->_bitmaps[idx]   = bmp;
            }
            if (idx < (int)self->_iconLoaded.size())
                self->_iconLoaded[idx] = true;
        }
        // Invalidate cached GDI+ bitmap so it's re-converted on next draw
        if (idx < (int)self->_gdiBitmaps.size()) {
            delete self->_gdiBitmaps[idx];
            self->_gdiBitmaps[idx] = nullptr;
        }
        self->DrawToLayeredWindow();
        return 0;
    }

    case WM_USER + 2: { // icon
        int   idx = (int)wParam;
        HICON ico = (HICON)lParam;
        {
            std::lock_guard<std::mutex> lk(self->_iconMutex);
            if (idx < (int)self->_icons.size()) {
                if (self->_icons[idx]) DestroyIcon(self->_icons[idx]);
                self->_icons[idx] = ico;
            }
            if (idx < (int)self->_iconLoaded.size())
                self->_iconLoaded[idx] = true;
        }
        // Invalidate cached GDI+ bitmap so it's re-converted on next draw
        if (idx < (int)self->_gdiBitmaps.size()) {
            delete self->_gdiBitmaps[idx];
            self->_gdiBitmaps[idx] = nullptr;
        }
        self->DrawToLayeredWindow();
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
