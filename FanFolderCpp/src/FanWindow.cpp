#include "pch.h"
#include "FanWindow.h"
#include "ShellDrag.h"

static constexpr float ArcSpreadPerItem   = 1.5f;
static constexpr float MaxArcSpreadDeg    = 22.0f;
static constexpr int   FormMargin         = 20;
static constexpr int   LabelGap           = 6;
static constexpr float HoverScaleMax      = 1.4f;
static constexpr float AnimSpeed_In       = 0.30f;
static constexpr float AnimSpeed_Out      = 0.38f;
static constexpr float EntryFadeDurationMs = 120.f;
static constexpr float ItemStageDurationMs = 28.f;
static constexpr float ItemAnimDurationMs  = 420.f;
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
      _folderPath(config.folderPath), _animStyle(config.animStyle),
      _items(std::move(items))
{}

FanWindow::~FanWindow() {
    if (_hwnd) {
        KillTimer(_hwnd, 1);
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
    std::lock_guard<std::mutex> lk(_iconMutex);
    for (auto h : _bitmaps) if (h) DeleteObject(h);
    for (auto h : _icons)   if (h) DestroyIcon(h);
}

bool FanWindow::Create() {
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
    return _hwnd != nullptr;
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
    _entryAlpha   = 0.f;
    _animating    = true;
    _createTick   = GetTickCount();

    if (_animStyle == ConfigData::AnimStyle::None) {
        std::fill(_entryProgress.begin(), _entryProgress.end(), 1.f);
        _entryAlpha = 1.f;
    }

    // Arrow item doesn't need async load
    _iconLoaded[total - 1] = true;
    for (int i = 0; i < (int)_items.size(); i++)
        StartIconLoad(i);

    DrawToLayeredWindow();
    ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
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
void FanWindow::CalculateLayout() {
    POINT cursor = {};
    GetCursorPos(&cursor);

    HMONITOR hMon = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);
    int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;

    _maxStackHeight = screenH * 0.75f;
    _iconSize = std::clamp(screenH / 19, 48, 128);

    // Measure label widths with GDI+
    Gdiplus::Bitmap tmpBmp(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics tmpG(&tmpBmp);
    float fontSize = _iconSize * 0.22f;
    Gdiplus::Font font(L"Segoe UI", fontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::StringFormat sfMeasure;
    sfMeasure.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    int total = (int)_items.size() + 1;
    _labelWidths.resize(total);
    int maxLabelW = 0;

    for (int i = 0; i < (int)_items.size(); i++) {
        Gdiplus::RectF bounds;
        tmpG.MeasureString(_items[i].name.c_str(), -1, &font, Gdiplus::PointF(0,0), &sfMeasure, &bounds);
        _labelWidths[i] = bounds.Width + 20.f;
        maxLabelW = std::max(maxLabelW, (int)_labelWidths[i]);
    }
    {
        const wchar_t* al = L"Open in Explorer";
        Gdiplus::RectF bounds;
        tmpG.MeasureString(al, -1, &font, Gdiplus::PointF(0,0), &sfMeasure, &bounds);
        _labelWidths[total - 1] = bounds.Width + 20.f;
        maxLabelW = std::max(maxLabelW, (int)_labelWidths[total - 1]);
    }

    float step = _iconSize * 1.2f;

    // Taskbar info
    APPBARDATA abd = { sizeof(abd) };
    SHAppBarMessage(ABM_GETTASKBARPOS, &abd);
    int tbH = abd.rc.bottom - abd.rc.top;
    int tbW = abd.rc.right  - abd.rc.left;
    bool taskbarAtBottom = tbH < tbW;

    // Window size
    int arcMargin  = (int)(_iconSize * std::sin(MaxArcSpreadDeg * 0.5f * kPI / 180.f) + 1.f);
    _winWidth  = FormMargin + maxLabelW + LabelGap + _iconSize + arcMargin + FormMargin;
    float stackH   = total * step + FormMargin * 2.f;
    if (stackH > _maxStackHeight) stackH = _maxStackHeight;
    _winHeight = (int)stackH;

    // Icon positions
    int iconCenterX = FormMargin + maxLabelW + LabelGap + _iconSize / 2;
    _labelOffsetX   = FormMargin;
    float totalArc  = std::min((float)total * ArcSpreadPerItem, MaxArcSpreadDeg);

    _iconPos.resize(total);
    _hitRects.resize(total);

    for (int i = 0; i < total; i++) {
        float t          = (total > 1) ? (float)i / (float)(total - 1) : 0.5f;
        float arcAngle   = (t - 0.5f) * totalArc;
        float arcOffsetX = std::sin(arcAngle * kPI / 180.f) * (_iconSize * 0.5f);
        float cx = (float)iconCenterX + arcOffsetX;
        float cy = _winHeight - FormMargin - _iconSize / 2.f - i * step;
        cy = std::max(cy, (float)(FormMargin + _iconSize / 2));

        _iconPos[i].x = (int)cx;
        _iconPos[i].y = (int)cy;

        int r = _iconSize / 2 + 4;
        _hitRects[i] = {
            std::max(0, (int)(cx - r) - maxLabelW - LabelGap),
            std::max(0, (int)(cy - r)),
            (int)(cx + r),
            (int)(cy + r)
        };
    }

    _arcOriginX = iconCenterX;
    _arcOriginY = _winHeight;

    // Screen position
    _winX = cursor.x - iconCenterX;
    if (_winX < mi.rcWork.left)              _winX = mi.rcWork.left;
    if (_winX + _winWidth > mi.rcWork.right) _winX = mi.rcWork.right - _winWidth;

    _winY = taskbarAtBottom ? (abd.rc.top - _winHeight) : abd.rc.bottom;
    if (_winY < mi.rcWork.top)                _winY = mi.rcWork.top;
    if (_winY + _winHeight > mi.rcWork.bottom) _winY = mi.rcWork.bottom - _winHeight;
}

// ---------------------------------------------------------------------------
void FanWindow::PremultiplyBitmap(Gdiplus::BitmapData& data) {
    auto* p = static_cast<BYTE*>(data.Scan0);
    for (UINT y = 0; y < data.Height; y++) {
        BYTE* row = p + y * data.Stride;
        for (UINT x = 0; x < data.Width; x++) {
            BYTE* px = row + x * 4;
            BYTE  a  = px[3];
            if (a == 0) {
                px[0] = px[1] = px[2] = 0;
            } else if (a < 255) {
                px[0] = (BYTE)((px[0] * a) / 255);
                px[1] = (BYTE)((px[1] * a) / 255);
                px[2] = (BYTE)((px[2] * a) / 255);
            }
        }
    }
}

void FanWindow::DrawToLayeredWindow() {
    if (!_hwnd || _winWidth <= 0 || _winHeight <= 0) return;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem    = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = _winWidth;
    bi.bmiHeader.biHeight      = -_winHeight;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hDIB = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hDIB) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        return;
    }
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hDIB);

    {
        Gdiplus::Bitmap bmp(_winWidth, _winHeight, PixelFormat32bppARGB);
        {
            Gdiplus::Graphics g(&bmp);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.Clear(Gdiplus::Color(0, 0, 0, 0));

            int total = (int)_items.size() + 1;
            for (int i = 0; i < total; i++) {
                float itemAlpha = 0.f;
                switch (_animStyle) {
                case ConfigData::AnimStyle::Spring: {
                    float ip = (i < (int)_itemProgress.size()) ? _itemProgress[i] : 0.f;
                    itemAlpha = std::clamp(ip, 0.f, 1.f) * _entryAlpha;
                    break;
                }
                case ConfigData::AnimStyle::Fan:
                case ConfigData::AnimStyle::Glide:
                    itemAlpha = (i < (int)_entryProgress.size()) ? _entryProgress[i] : 0.f;
                    break;
                case ConfigData::AnimStyle::None:
                    itemAlpha = 1.f;
                    break;
                }
                DrawItem(g, i, itemAlpha);
            }
        }

        Gdiplus::Rect rect(0, 0, _winWidth, _winHeight);
        Gdiplus::BitmapData bd;
        if (bmp.LockBits(&rect,
                         Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite,
                         PixelFormat32bppARGB, &bd) == Gdiplus::Ok) {
            PremultiplyBitmap(bd);
            auto* src = static_cast<BYTE*>(bd.Scan0);
            auto* dst = static_cast<BYTE*>(pBits);
            for (int y = 0; y < _winHeight; y++)
                memcpy(dst + y * _winWidth * 4, src + y * bd.Stride, _winWidth * 4);
            bmp.UnlockBits(&bd);
        }
    }

    POINT ptSrc = {0, 0};
    SIZE  szWin = {_winWidth, _winHeight};
    POINT ptDst = {_winX, _winY};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(_hwnd, hdcScreen, &ptDst, &szWin, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOld);
    DeleteObject(hDIB);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

// ---------------------------------------------------------------------------
void FanWindow::DrawShellBitmapIA(Gdiplus::Graphics& g, HBITMAP hBmp,
                                   float x, float y, float size,
                                   Gdiplus::ImageAttributes* ia) {
    BITMAP bm = {};
    GetObject(hBmp, sizeof(bm), &bm);
    if (bm.bmWidth <= 0) return;

    HDC     hdc  = CreateCompatibleDC(nullptr);
    HBITMAP hOld = (HBITMAP)SelectObject(hdc, hBmp);

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = bm.bmWidth;
    bi.bmiHeader.biHeight      = -bm.bmHeight;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    std::vector<BYTE> bits(bm.bmWidth * bm.bmHeight * 4);
    GetDIBits(hdc, hBmp, 0, bm.bmHeight, bits.data(), &bi, DIB_RGB_COLORS);
    SelectObject(hdc, hOld);
    DeleteDC(hdc);

    Gdiplus::Bitmap bmpG(bm.bmWidth, bm.bmHeight, bm.bmWidth * 4,
                         PixelFormat32bppPARGB, bits.data());
    Gdiplus::RectF dest(x, y, size, size);
    if (ia) {
        g.DrawImage(&bmpG, dest, 0, 0,
                    (Gdiplus::REAL)bmpG.GetWidth(), (Gdiplus::REAL)bmpG.GetHeight(),
                    Gdiplus::UnitPixel, ia);
    } else {
        g.DrawImage(&bmpG, dest);
    }
}

void FanWindow::DrawShellBitmap(Gdiplus::Graphics& g, HBITMAP hBmp,
                                   float x, float y, float size) {
    DrawShellBitmapIA(g, hBmp, x, y, size, nullptr);
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

    float fontSize = _iconSize * 0.22f;
    Gdiplus::Font font(L"Segoe UI", fontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color((BYTE)(255.f * alpha), 255, 255, 255));
    Gdiplus::StringFormat sf;
    sf.SetAlignment(Gdiplus::StringAlignmentFar);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    sf.SetTrimming(Gdiplus::StringTrimmingNone);

    Gdiplus::RectF textRect(pillLeft + 8.f, pillTop, pillW - 16.f, pillH);
    g.DrawString(text.c_str(), -1, &font, textRect, &sf, &textBrush);
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

void FanWindow::DrawItem(Gdiplus::Graphics& g, int idx, float itemAlpha) {
    if (itemAlpha <= 0.f) return;
    if (idx >= (int)_iconPos.size()) return;

    float hsc    = (idx < (int)_hoverScale.size()) ? _hoverScale[idx] : 1.f;
    float cx     = (float)_iconPos[idx].x;
    float cy     = (float)_iconPos[idx].y;
    float drawSz = (float)_iconSize * hsc;
    float entryP = (idx < (int)_entryProgress.size()) ? _entryProgress[idx] : 1.f;

    switch (_animStyle) {
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

    bool hoverActive = (idx < (int)_hoverScale.size() && _hoverScale[idx] > 1.01f);
    if (!isArrow && hoverActive && bmp != nullptr) {
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
            Gdiplus::ImageAttributes ia;
            ia.SetColorMatrix(&shadowCm, Gdiplus::ColorMatrixFlagsDefault,
                              Gdiplus::ColorAdjustTypeBitmap);
            float offsets[] = {-pass.offset, 0.f, pass.offset};
            for (float ox : offsets) for (float oy : offsets) {
                if (ox == 0.f && oy == 0.f) continue;
                DrawShellBitmapIA(g, bmp, iconX + ox, iconY + oy, drawSz, &ia);
            }
        }
    }

    if (bmp) {
        DrawShellBitmap(g, bmp, iconX, iconY, drawSz);
    } else if (ico) {
        Gdiplus::Bitmap* iconBmp = Gdiplus::Bitmap::FromHICON(ico);
        if (iconBmp) {
            Gdiplus::RectF dest(iconX, iconY, drawSz, drawSz);
            Gdiplus::ColorMatrix cm = {
                1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0,
                0,0,0,itemAlpha,0, 0,0,0,0,1
            };
            Gdiplus::ImageAttributes ia;
            ia.SetColorMatrix(&cm, Gdiplus::ColorMatrixFlagsDefault,
                              Gdiplus::ColorAdjustTypeBitmap);
            g.DrawImage(iconBmp, dest, 0, 0,
                        (Gdiplus::REAL)iconBmp->GetWidth(),
                        (Gdiplus::REAL)iconBmp->GetHeight(),
                        Gdiplus::UnitPixel, &ia);
            delete iconBmp;
        }
    } else {
        // Placeholder
        Gdiplus::SolidBrush ph(Gdiplus::Color((BYTE)(80.f * itemAlpha), 150, 150, 150));
        g.FillRectangle(&ph, Gdiplus::RectF(iconX, iconY, drawSz, drawSz));
    }

    // Label pill
    std::wstring name = _items[idx].name;
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
        if (!_folderPath.empty())
            ShellExecuteW(nullptr, L"open", L"explorer.exe",
                          _folderPath.c_str(), nullptr, SW_SHOWNORMAL);
    } else if (idx >= 0 && idx < (int)_items.size()) {
        ShellExecuteW(nullptr, L"open", _items[idx].fullPath.c_str(),
                      nullptr, nullptr, SW_SHOWNORMAL);
    }
    Close();
}

void FanWindow::ShowContextMenu(int idx, POINT screenPt) {
    const std::wstring& path = (idx >= 0 && idx < (int)_items.size())
        ? _items[idx].fullPath : _folderPath;
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
        HBITMAP bmp = FileService::GetShellBitmap(p, sz);
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

            switch (self->_animStyle) {
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
                for (int i = 0; i < total && i < (int)self->_entryProgress.size(); i++) {
                    float stagger = i * 30.f;
                    float t = std::clamp((elapsed - stagger) / 330.f, 0.f, 1.f);
                    float u = 1.f - t;
                    float eased = 1.f - u * u * u * u;
                    if (eased != self->_entryProgress[i]) { self->_entryProgress[i] = eased; dirty = true; }
                }
                break;
            }
            case ConfigData::AnimStyle::Glide: {
                float t = std::clamp(elapsed / 800.f, 0.f, 1.f);
                float u = 1.f - t;
                float eased = 1.f - u * u * u;
                for (int i = 0; i < total && i < (int)self->_entryProgress.size(); i++) {
                    if (eased != self->_entryProgress[i]) { self->_entryProgress[i] = eased; dirty = true; }
                }
                break;
            }
            case ConfigData::AnimStyle::None:
                break;
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
        }
        return 0;

    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        if ((wParam & MK_LBUTTON) && self->_dragIdx >= 0 && !self->_dragging) {
            int dx = x - self->_dragStart.x, dy = y - self->_dragStart.y;
            if (dx * dx + dy * dy > 25) {
                self->_dragging = true;
                if (self->_dragIdx < (int)self->_items.size())
                    DoShellDrag(hwnd, self->_items[self->_dragIdx].fullPath);
                self->_dragIdx  = -1;
                self->_dragging = false;
            }
        }

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
            self->_dragIdx   = hit;
            self->_dragStart = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            self->_dragging  = false;
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        if (!self->_dragging) {
            int hit = self->HitTest(x, y);
            if (hit >= 0 && hit == self->_dragIdx) {
                self->LaunchItem(hit);
                return 0;
            }
        }
        self->_dragIdx  = -1;
        self->_dragging = false;
        return 0;
    }

    case WM_RBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int hit = self->HitTest(pt.x, pt.y);
        ClientToScreen(hwnd, &pt);
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
        self->DrawToLayeredWindow();
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
