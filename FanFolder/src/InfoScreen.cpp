// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "InfoScreen.h"
#include "Localization.h"

// Link URLs (same for all locales).
static const wchar_t* const kLinkUrls[3] = {
    L"https://buymeacoffee.com/fanfolder",
    L"https://olebhartvigsen.github.io/FanFolder/",
    L"https://github.com/olebhartvigsen/FanFolder/issues/new",
};

// Small line-drawn glyphs shown in the box left of each link row.
enum class LinkGlyph { Heart, Home, Bug };

// ---------------------------------------------------------------------------
void InfoScreen::Register(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = ClassName();
    RegisterClassExW(&wc);
}

InfoScreen::~InfoScreen() {
    if (_hwnd) {
        DestroyWindow(_hwnd);
        _hwnd = nullptr;
    }
    delete _titleFont; _titleFont = nullptr;
    delete _bodyFont;  _bodyFont  = nullptr;
    delete _linkFont;  _linkFont  = nullptr;
    delete _hintFont;  _hintFont  = nullptr;
    FreeBackBuffer();
}

bool InfoScreen::Create(HWND hwndParent) {
    _hwndParent = hwndParent;

    // Layered, no-activate, topmost, tool window: same window family as the
    // fan.  The panel never steals focus and stays out of Alt+Tab.
    _hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        ClassName(), L"", WS_POPUP,
        0, 0, 1, 1,
        nullptr, nullptr, _hInst, this);
    return _hwnd != nullptr;
}

void InfoScreen::Show() {
    if (!_hwnd) return;
    CalculateLayout();
    EnsureBackBuffer();

    // Center on the primary monitor's work area, a bit above true center so
    // it sits clear of the taskbar.  SW_SHOWNOACTIVATE keeps the current
    // foreground app focused.
    HMONITOR hMon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);
    int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - _winW) / 2;
    int y = mi.rcWork.top  + (((mi.rcWork.bottom - mi.rcWork.top) - _winH) * 2) / 5;

    SetWindowPos(_hwnd, HWND_TOPMOST, x, y, _winW, _winH,
                 SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Cache the card's screen rect for the LL mouse hook (click-away close).
    RECT rcCard = { x + (int)_cardRect.X, y + (int)_cardRect.Y,
                    x + (int)(_cardRect.X + _cardRect.Width),
                    y + (int)(_cardRect.Y + _cardRect.Height) };
    _cardScreenRect = rcCard;
    _cardRectValid  = true;

    _fadeAlpha = 0.f;
    ShowWindow(_hwnd, SW_SHOWNOACTIVATE);
    Draw();
    SetTimer(_hwnd, kFadeTimerId, 16, nullptr);
}

void InfoScreen::Close() {
    if (!_hwnd) return;
    KillTimer(_hwnd, kFadeTimerId);
    ShowWindow(_hwnd, SW_HIDE);
    _cardRectValid = false;
}

bool InfoScreen::IsPointOnPanel(POINT screenPt) const {
    if (!_cardRectValid) return false;
    return screenPt.x >= _cardScreenRect.left  && screenPt.x <  _cardScreenRect.right &&
           screenPt.y >= _cardScreenRect.top   && screenPt.y <  _cardScreenRect.bottom;
}

// ---------------------------------------------------------------------------
// Layout constants in 96-DPI px, scaled by _scale/100 at build time.
namespace {
    constexpr float kCardW  = 440.f;   // minimum card width
    constexpr float kMargin = 16.f;   // transparent margin around the card
    constexpr float kPadX  = 28.f;    // inner card side padding
    constexpr float kTit   = 20.f;    // title, pt
    constexpr float kBody  = 11.5f;   // body text, pt
    constexpr float kLink  = 12.f;    // link label, pt
    constexpr float kHnt   = 10.f;    // dismiss hint, pt
    constexpr float kRowH  = 34.f;    // link row height
    constexpr float kIcon  = 20.f;    // glyph box size
    constexpr float kGapIc = 10.f;    // glyph to label gap
}

void InfoScreen::RebuildFonts() {
    delete _titleFont; _titleFont = nullptr;
    delete _bodyFont;  _bodyFont  = nullptr;
    delete _linkFont;  _linkFont  = nullptr;
    delete _hintFont;  _hintFont  = nullptr;

    const float s = (float)_scale / 100.f;
    _titleFont = new Gdiplus::Font(L"Segoe UI", kTit  * s, Gdiplus::FontStyleBold,    Gdiplus::UnitPoint);
    _bodyFont  = new Gdiplus::Font(L"Segoe UI", kBody * s, Gdiplus::FontStyleRegular,  Gdiplus::UnitPoint);
    _linkFont  = new Gdiplus::Font(L"Segoe UI", kLink * s, Gdiplus::FontStyleRegular,  Gdiplus::UnitPoint);
    _hintFont  = new Gdiplus::Font(L"Segoe UI", kHnt  * s, Gdiplus::FontStyleRegular,  Gdiplus::UnitPoint);
    _fontsBuiltScale = _scale;
}

void InfoScreen::CalculateLayout() {
    const InfoStrings& is = GetInfoStrings();

    // DPI scale of the primary monitor (shcore is always present on 2004+).
    HMONITOR hMon = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    UINT dpi = 96;
    {
        HMODULE hShCore = GetModuleHandleW(L"shcore.dll");
        if (hShCore) {
            auto pGetDpiForMonitor = reinterpret_cast<HRESULT (WINAPI*)(HMONITOR, int, UINT*, UINT*)>(
                GetProcAddress(hShCore, "GetDpiForMonitor"));
            if (pGetDpiForMonitor)
                pGetDpiForMonitor(hMon, 0 /*MDT_EFFECTIVE_DPI*/, &dpi, &dpi);
        }
    }
    _scale = (int)((dpi * 100 + 48) / 96);
    const float s = (float)_scale / 100.f;

    if (_fontsBuiltScale != _scale || _linkLabels.size() != (size_t)kLinkCount) {
        if (_fontsBuiltScale != _scale)
            RebuildFonts();
        _linkLabels = { is.supportLink, is.homepageLink, is.reportLink };
    }

    // Measure text with a tiny offscreen bitmap (FanWindow's pattern).
    Gdiplus::Bitmap measureBmp(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics mg(measureBmp);
    Gdiplus::StringFormat noWrap;
    noWrap.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    const float padX  = kPadX * s;
    const float rowH  = kRowH * s;
    const float icon  = kIcon * s;
    const float gapIc = kGapIc * s;
    const float contentW = kCardW * s - 2.f * padX;

    auto textW = [&](const wchar_t* t, Gdiplus::Font* f) -> float {
        if (!f || !t || !t[0]) return 0.f;
        Gdiplus::RectF b;
        mg.MeasureString(t, -1, f, Gdiplus::PointF(0, 0), &noWrap, &b);
        return b.Width;
    };

    // About text: measure its true wrapped height against the content width
    // (RectF layout makes GDI+ wrap; the out-box reports the real bounds).
    auto wrappedH = [&](const wchar_t* t, Gdiplus::Font* f) -> float {
        if (!f || !t || !t[0]) return 0.f;
        Gdiplus::StringFormat wsf;   // default wrapping behaviour
        Gdiplus::RectF layoutRect(0, 0, contentW, 10000.f);
        Gdiplus::RectF bounds;
        if (mg.MeasureString(t, -1, f, layoutRect, &wsf, &bounds) == Gdiplus::Ok)
            return bounds.Height;
        return 0.f;
    };

    float bodyH = wrappedH(is.aboutText, _bodyFont);
    if (bodyH <= 0.f) bodyH = kBody * s * 1.55f * 2.f;   // fallback: two lines

    float maxLinkW = 0.f;
    for (auto& lbl : _linkLabels)
        maxLinkW = std::max(maxLinkW, textW(lbl.c_str(), _linkFont));
    const float rowW = icon + gapIc + maxLinkW + 2.f;

    // Heights of the single-line rows, measured so GDI+'s point-to-pixel
    // conversion (pt * dpi/72) can never clip the text.
    const float titleH = std::max(wrappedH(L"FanFolder", _titleFont), kTit * s * 1.3f) + 2.f;
    const float linkLineH = std::max(wrappedH(_linkLabels.empty() ? L"" : _linkLabels[0].c_str(), _linkFont),
                                     kLink * s * 1.3f);
    const float hintH = std::max(wrappedH(is.clickHint, _hintFont), kHnt * s * 1.3f) + 2.f;

    const float gap1 = 4.f  * s;   // title -> body
    const float gap2 = 14.f * s;   // body  -> links
    const float gap3 = 8.f  * s;   // links -> hint
    const float padTop = 24.f * s;
    const float padBottom = 18.f * s;

    const float cardW = std::max(kCardW * s, rowW + 2.f * padX + 8.f);
    const float cardH = padTop
                        + titleH + gap1
                        + bodyH + gap2
                        + kLinkCount * rowH
                        + gap3
                        + hintH
                        + padBottom;

    _cardRect = Gdiplus::RectF(kMargin * s, kMargin * s, cardW, cardH);

    const float x0 = _cardRect.X + padX;
    float y = _cardRect.Y + padTop;

    _titleRect = Gdiplus::RectF(x0, y, cardW - 2.f * padX, titleH); y += titleH + gap1;
    _bodyRect  = Gdiplus::RectF(x0, y, cardW - 2.f * padX, bodyH);  y += bodyH + gap2;

    _linkRects.clear();
    _iconRects.clear();
    _linkTextRects.clear();
    for (int i = 0; i < kLinkCount; i++) {
        _linkRects.emplace_back(x0, y, rowW, rowH);
        _iconRects.emplace_back(x0, y + (rowH - icon) / 2.f, icon, icon);
        _linkTextRects.emplace_back(x0 + icon + gapIc, y,
                                    rowW - icon - gapIc, rowH);
        y += rowH;
    }

    y += gap3;
    _hintRect = Gdiplus::RectF(_cardRect.X, y, cardW, hintH);

    _winW = (int)std::ceil(_cardRect.X + _cardRect.Width  + kMargin * s);
    _winH = (int)std::ceil(_cardRect.Y + _cardRect.Height + kMargin * s);
}

// ---------------------------------------------------------------------------
void InfoScreen::EnsureBackBuffer() {
    if (_hBackDIB && _backW == _winW && _backH == _winH) return;
    FreeBackBuffer();

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = _winW;
    bi.bmiHeader.biHeight      = -_winH;   // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC hdcScreen = GetDC(nullptr);
    _hBackDIB = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &_pBackBits, nullptr, 0);
    _hdcBack  = CreateCompatibleDC(hdcScreen);
    ReleaseDC(nullptr, hdcScreen);
    if (!_hBackDIB || !_hdcBack) { FreeBackBuffer(); return; }
    SelectObject(_hdcBack, _hBackDIB);

    _backBmp = new Gdiplus::Bitmap(_winW, _winH, _winW * 4,
                                   PixelFormat32bppARGB, (BYTE*)_pBackBits);
    if (_backBmp->GetLastStatus() != Gdiplus::Ok) {
        delete _backBmp; _backBmp = nullptr;
        FreeBackBuffer();
        return;
    }
    _backW = _winW;
    _backH = _winH;
}

void InfoScreen::FreeBackBuffer() {
    delete _backBmp;   _backBmp   = nullptr;
    if (_hdcBack)  { DeleteDC(_hdcBack);      _hdcBack  = nullptr; }
    if (_hBackDIB) { DeleteObject(_hBackDIB); _hBackDIB = nullptr; }
    _pBackBits = nullptr;
    _backW = _backH = 0;
}

// ---------------------------------------------------------------------------
static void DrawRoundedRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& r, float radius) {
    float d = radius * 2.f;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
}

static Gdiplus::RectF ExpandRect(const Gdiplus::RectF& r, float dx, float dy) {
    return Gdiplus::RectF(r.X - dx, r.Y - dy, r.Width + 2.f * dx, r.Height + 2.f * dy);
}

static void DrawGlyph(Gdiplus::Graphics& g, LinkGlyph gl, const Gdiplus::RectF& rc,
                      const Gdiplus::Color& color, float scale) {
    Gdiplus::Pen pen(color, 1.8f * scale);
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
    const float cx = rc.X + rc.Width  / 2.f;
    const float cy = rc.Y + rc.Height / 2.f;
    const float w  = rc.Width;
    const float h  = rc.Height;

    switch (gl) {
    case LinkGlyph::Heart:   // two lobes + a V down to the tip
        g.DrawEllipse(&pen, cx - w * 0.30f, cy - h * 0.32f, w * 0.28f, h * 0.28f);
        g.DrawEllipse(&pen, cx + w * 0.02f, cy - h * 0.32f, w * 0.28f, h * 0.28f);
        g.DrawLine(&pen, cx - w * 0.29f, cy - h * 0.10f, cx, cy + h * 0.32f);
        g.DrawLine(&pen, cx + w * 0.29f, cy - h * 0.10f, cx, cy + h * 0.32f);
        break;
    case LinkGlyph::Home:    // roof + walls + floor
        g.DrawLine(&pen, cx - w * 0.28f, cy - h * 0.02f, cx, cy - h * 0.28f);
        g.DrawLine(&pen, cx, cy - h * 0.28f, cx + w * 0.28f, cy - h * 0.02f);
        g.DrawLine(&pen, cx - w * 0.28f, cy - h * 0.02f, cx - w * 0.22f, cy + h * 0.02f);
        g.DrawLine(&pen, cx + w * 0.28f, cy - h * 0.02f, cx + w * 0.22f, cy + h * 0.02f);
        g.DrawLine(&pen, cx - w * 0.22f, cy + h * 0.02f, cx - w * 0.22f, cy + h * 0.30f);
        g.DrawLine(&pen, cx + w * 0.22f, cy + h * 0.02f, cx + w * 0.22f, cy + h * 0.30f);
        g.DrawLine(&pen, cx - w * 0.22f, cy + h * 0.30f, cx + w * 0.22f, cy + h * 0.30f);
        break;
    case LinkGlyph::Bug:     // body + seam + antenna
        g.DrawEllipse(&pen, cx - w * 0.28f, cy - h * 0.28f, w * 0.56f, h * 0.56f);
        g.DrawLine(&pen, cx - w * 0.28f, cy, cx + w * 0.28f, cy);
        g.DrawLine(&pen, cx, cy - h * 0.28f, cx, cy - h * 0.45f);
        break;
    }
}

void InfoScreen::Draw() {
    if (!_hwnd || !_backBmp || !_pBackBits) return;

    const InfoStrings& is = GetInfoStrings();
    const float s = (float)_scale / 100.f;

    std::memset(_pBackBits, 0, (size_t)_winW * _winH * 4);

    Gdiplus::Graphics g(_backBmp);
    if (g.GetLastStatus() != Gdiplus::Ok) return;
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
    const BYTE A = (BYTE)(255.f * _fadeAlpha);

    // Card fill + hairline border
    Gdiplus::GraphicsPath card;
    DrawRoundedRect(card, _cardRect, 12.f * s);
    Gdiplus::SolidBrush cardBrush(Gdiplus::Color(A, 24, 24, 28));
    g.FillPath(&cardBrush, &card);
    Gdiplus::Pen cardPen(Gdiplus::Color((BYTE)(A * 60 / 255), 70, 70, 80), 1.f);
    g.DrawPath(&cardPen, &card);

    // Title
    Gdiplus::SolidBrush titleBrush(Gdiplus::Color(A, 255, 255, 255));
    if (_titleFont) {
        Gdiplus::StringFormat tsf;
        tsf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        tsf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        g.DrawString(L"FanFolder", 9, _titleFont, _titleRect, &tsf, &titleBrush);
    }

    // About text (rect sized for up to two wrapped lines in CalculateLayout)
    Gdiplus::SolidBrush bodyBrush(Gdiplus::Color((BYTE)(A * 228 / 255), 225, 225, 230));
    if (_bodyFont) {
        Gdiplus::StringFormat bsf;
        bsf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        bsf.SetLineAlignment(Gdiplus::StringAlignmentNear);
        bsf.SetAlignment(Gdiplus::StringAlignmentNear);
        g.DrawString(is.aboutText, -1, _bodyFont, _bodyRect, &bsf, &bodyBrush);
    }

    // Link rows: glyph box + colored label, hover highlight.
    static const LinkGlyph glyphs[kLinkCount] = { LinkGlyph::Heart, LinkGlyph::Home, LinkGlyph::Bug };
    for (int i = 0; i < kLinkCount; i++) {
        const Gdiplus::RectF& row = _linkRects[i];
        if (i == _hoverLink) {
            Gdiplus::GraphicsPath rp;
            DrawRoundedRect(rp, ExpandRect(row, 4.f * s, 3.f * s), 8.f * s);
            Gdiplus::SolidBrush hb(Gdiplus::Color(A, 44, 44, 52));
            g.FillPath(&hb, &rp);
        }

        Gdiplus::Color accent;   // warm coffee / web blue / bug orange
        switch (i) {
        case 0:  accent = Gdiplus::Color(A, 255, 190, 100); break;
        case 1:  accent = Gdiplus::Color(A, 120, 170, 255); break;
        default: accent = Gdiplus::Color(A, 255, 140, 100); break;
        }

        DrawGlyph(g, glyphs[i], _iconRects[i], accent, s);

        if (_linkFont) {
            Gdiplus::SolidBrush linkBrush(accent);
            Gdiplus::StringFormat lsf;
            lsf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
            lsf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            lsf.SetAlignment(Gdiplus::StringAlignmentNear);
            g.DrawString(_linkLabels[i].c_str(), -1, _linkFont, _linkTextRects[i], &lsf, &linkBrush);
        }
    }

    // Dismiss hint, centered across the card
    Gdiplus::SolidBrush hintBrush(Gdiplus::Color((BYTE)(A * 160 / 255), 150, 150, 160));
    if (_hintFont) {
        Gdiplus::StringFormat hsf;
        hsf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        hsf.SetAlignment(Gdiplus::StringAlignmentCenter);
        hsf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(is.clickHint, -1, _hintFont, _hintRect, &hsf, &hintBrush);
    }

    // Premultiply alpha in place, then push the frame.
    {
        BYTE* px = static_cast<BYTE*>(_pBackBits);
        const int stride = _winW * 4;
        for (int y = 0; y < _winH; y++) {
            BYTE* rowP = px + y * stride;
            for (int x = 0; x < _winW; x++, rowP += 4) {
                BYTE a = rowP[3];
                if (a == 0) {
                    rowP[0] = rowP[1] = rowP[2] = 0;
                } else if (a < 255) {
                    rowP[0] = (BYTE)((rowP[0] * a + 128) >> 8);
                    rowP[1] = (BYTE)((rowP[1] * a + 128) >> 8);
                    rowP[2] = (BYTE)((rowP[2] * a + 128) >> 8);
                }
            }
        }
    }

    HDC hdcScreen = GetDC(nullptr);
    POINT ptSrc = { 0, 0 };
    SIZE  szWin = { _winW, _winH };
    POINT ptDst = { 0, 0 };
    RECT rc;
    if (GetWindowRect(_hwnd, &rc)) ptDst = { rc.left, rc.top };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(_hwnd, hdcScreen, &ptDst, &szWin, _hdcBack, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, hdcScreen);
}

// ---------------------------------------------------------------------------
int InfoScreen::HitTest(int x, int y) const {
    for (int i = 0; i < (int)_linkRects.size(); i++) {
        const Gdiplus::RectF& r = _linkRects[i];
        if (x >= r.X && x < r.X + r.Width && y >= r.Y && y < r.Y + r.Height)
            return i;
    }
    return -1;
}

void InfoScreen::LaunchLink(int idx) {
    if (idx < 0 || idx >= kLinkCount) return;
    ShellExecuteW(nullptr, L"open", kLinkUrls[idx], nullptr, nullptr, SW_SHOWNORMAL);
}

// ---------------------------------------------------------------------------
LRESULT CALLBACK InfoScreen::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    InfoScreen* self = FromHWND(hwnd);
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_TIMER:
        if (wParam == kFadeTimerId) {
            if (self->_fadeAlpha < 1.f) {
                self->_fadeAlpha = std::min(1.f, self->_fadeAlpha + 0.083f);
                self->Draw();
            }
            if (self->_fadeAlpha >= 1.f)
                KillTimer(hwnd, kFadeTimerId);
            return 0;
        }
        break;

    case WM_MOUSEMOVE: {
        int hit = self->HitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (hit != self->_hoverLink) {
            self->_hoverLink = hit;
            SetCursor(LoadCursor(nullptr, hit >= 0 ? IDC_HAND : IDC_ARROW));
            self->Draw();
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        if (self->_hoverLink != -1) {
            self->_hoverLink = -1;
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            self->Draw();
        }
        return 0;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        self->_downLink = self->HitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        int hit = self->HitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        self->_downLink = -1;
        if (hit >= 0) {
            self->LaunchLink(hit);   // link click: browser opens, panel stays
        } else {
            // Body click: route through the owner so hook teardown happens
            // in one place (MainWindow::CloseInfoScreen).
            if (self->_hwndParent)
                PostMessageW(self->_hwndParent, WM_INFO_DISMISSED, 0, 0);
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
