// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "InfoScreen.h"
#include "Localization.h"
#include "../resources/resource.h"

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
    delete _appIcon;   _appIcon   = nullptr;
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
    _hoverLink = -1;
    _hoverClose = false;
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
    constexpr float kCardW   = 460.f;  // minimum card width
    constexpr float kMargin  = 16.f;   // transparent margin around the card
    constexpr float kPadX    = 30.f;   // inner card side padding
    constexpr float kTit     = 20.f;   // title, pt
    constexpr float kBody    = 11.5f;  // body text, pt
    constexpr float kLink    = 12.f;   // link label, pt
    constexpr float kHnt     = 10.f;   // dismiss hint, pt
    constexpr float kRowH    = 46.f;   // link row height
    constexpr float kIcon    = 30.f;   // tinted tile size
    constexpr float kGapIc   = 12.f;   // tile to label gap
    constexpr float kAppIcon = 44.f;   // header app icon
    constexpr float kGapApp  = 14.f;   // app icon to title gap
    constexpr float kClose   = 26.f;   // close button
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

void InfoScreen::EnsureAppIcon() {
    const int px = (int)std::lround(kAppIcon * ((float)_scale / 100.f));
    if (_appIcon && _appIconPx == px) return;

    delete _appIcon;
    _appIcon = nullptr;
    _appIconPx = 0;

    HICON hIcon = (HICON)LoadImageW(_hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                    px, px, LR_DEFAULTCOLOR);
    if (!hIcon) return;

    _appIcon = Gdiplus::Bitmap::FromHICON(hIcon);
    DestroyIcon(hIcon);
    if (_appIcon && _appIcon->GetLastStatus() != Gdiplus::Ok) {
        delete _appIcon;
        _appIcon = nullptr;
    }
    if (_appIcon) _appIconPx = px;
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
    EnsureAppIcon();

    // Measure text with a tiny offscreen bitmap (FanWindow's pattern).
    Gdiplus::Bitmap measureBmp(1, 1, PixelFormat32bppARGB);
    Gdiplus::Graphics mg(&measureBmp);
    Gdiplus::StringFormat noWrap;
    noWrap.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    const float padX     = kPadX * s;
    const float rowH     = kRowH * s;
    const float icon     = kIcon * s;
    const float gapIc    = kGapIc * s;
    const float appIcon  = kAppIcon * s;
    const float gapApp   = kGapApp * s;
    const float close    = kClose * s;
    const float padTop   = 24.f * s;
    const float padBottom = 20.f * s;
    const float rowGap   = 8.f * s;

    auto textW = [&](const wchar_t* t, Gdiplus::Font* f) -> float {
        if (!f || !t || !t[0]) return 0.f;
        Gdiplus::RectF b;
        mg.MeasureString(t, -1, f, Gdiplus::PointF(0, 0), &noWrap, &b);
        return b.Width;
    };

    float maxLinkW = 0.f;
    for (auto& lbl : _linkLabels)
        maxLinkW = std::max(maxLinkW, textW(lbl.c_str(), _linkFont));

    // Rows are full-width cards: 14 px inset, tile, label, chevron area.
    const float rowContentW = 14.f * s + icon + gapIc + maxLinkW + 28.f * s;
    const float cardW = std::max(kCardW * s, rowContentW + 2.f * padX);
    const float contentW = cardW - 2.f * padX;

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

    const float titleH  = std::max(wrappedH(L"FanFolder", _titleFont), kTit * s * 1.3f) + 2.f;
    const float hintH   = std::max(wrappedH(is.clickHint, _hintFont), kHnt * s * 1.3f) + 2.f;
    const float headerH = std::max(appIcon, titleH);

    const float cardTop  = kMargin * s;
    const float headerY  = cardTop + padTop;
    const float dividerY = headerY + headerH + 10.f * s;
    const float bodyY    = dividerY + 18.f * s;
    const float linksY   = bodyY + bodyH + 20.f * s;
    const float hintY    = linksY + kLinkCount * rowH + (kLinkCount - 1) * rowGap + 14.f * s;
    const float cardH    = (hintY + hintH + padBottom) - cardTop;

    _cardRect = Gdiplus::RectF(kMargin * s, cardTop, cardW, cardH);
    _dividerY = dividerY;

    const float x0 = _cardRect.X + padX;
    const float titleW = std::max(40.f * s, contentW - appIcon - gapApp - close - 10.f * s);

    _appIconRect = Gdiplus::RectF(x0, headerY + (headerH - appIcon) / 2.f, appIcon, appIcon);
    _titleRect   = Gdiplus::RectF(x0 + appIcon + gapApp,
                                  headerY + (headerH - titleH) / 2.f,
                                  titleW, titleH);
    _closeRect   = Gdiplus::RectF(_cardRect.X + cardW - padX - close,
                                  headerY + (headerH - close) / 2.f,
                                  close, close);
    _bodyRect    = Gdiplus::RectF(x0, bodyY, contentW, bodyH);

    _linkRects.clear();
    _iconRects.clear();
    _linkTextRects.clear();
    float y = linksY;
    for (int i = 0; i < kLinkCount; i++) {
        _linkRects.emplace_back(x0, y, contentW, rowH);
        _iconRects.emplace_back(x0 + 14.f * s, y + (rowH - icon) / 2.f, icon, icon);
        _linkTextRects.emplace_back(x0 + 14.f * s + icon + gapIc, y,
                                    contentW - 14.f * s - icon - gapIc - 28.f * s, rowH);
        y += rowH;
        if (i + 1 < kLinkCount) y += rowGap;
    }

    _hintRect = Gdiplus::RectF(_cardRect.X, hintY, cardW, hintH);

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
    // GridFit snaps glyph stems to the pixel grid instead of pure subpixel
    // blending, which reads sharper at normal UI sizes on a layered window
    // (no ClearType there, so plain AntiAlias text tends to look soft/hazy).
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    const BYTE A = (BYTE)(255.f * _fadeAlpha);
    const float x0 = _bodyRect.X;

    // Card: a dark navy gradient carrying the site's brand tone, not a flat
    // near-black. Noticeably lighter than the old charcoal (31,31,40) so the
    // panel doesn't read as plain black, with a faint blue undertone.
    Gdiplus::GraphicsPath card;
    DrawRoundedRect(card, _cardRect, 16.f * s);
    Gdiplus::LinearGradientBrush cardBrush(
        Gdiplus::PointF(_cardRect.X, _cardRect.Y),
        Gdiplus::PointF(_cardRect.X, _cardRect.Y + _cardRect.Height),
        Gdiplus::Color(A, 40, 42, 58),
        Gdiplus::Color(A, 24, 25, 38));
    g.FillPath(&cardBrush, &card);
    Gdiplus::Pen cardPen(Gdiplus::Color((BYTE)(A * 100 / 255), 96, 100, 128), 1.f);
    g.DrawPath(&cardPen, &card);

    // Thin brand gradient along the top edge, clipped to the rounded card.
    {
        g.SetClip(&card);
        Gdiplus::Color stops[3] = {
            Gdiplus::Color(A, 92, 224, 140),
            Gdiplus::Color(A, 127, 176, 255),
            Gdiplus::Color(A, 200, 165, 255),
        };
        Gdiplus::REAL positions[3] = { 0.f, 0.5f, 1.f };
        Gdiplus::LinearGradientBrush accent(
            Gdiplus::PointF(_cardRect.X, _cardRect.Y),
            Gdiplus::PointF(_cardRect.X + _cardRect.Width, _cardRect.Y),
            stops[0], stops[2]);
        accent.SetInterpolationColors(stops, positions, 3);
        Gdiplus::Pen accentPen(&accent, 2.f * s);
        g.DrawLine(&accentPen,
                   _cardRect.X, _cardRect.Y + 2.f * s,
                   _cardRect.X + _cardRect.Width, _cardRect.Y + 2.f * s);
        g.ResetClip();
    }

    // Header: app icon, title, close button.
    if (_appIcon) {
        g.DrawImage(_appIcon, _appIconRect);
    } else {
        Gdiplus::GraphicsPath ip;
        DrawRoundedRect(ip, _appIconRect, 10.f * s);
        Gdiplus::LinearGradientBrush ib(
            Gdiplus::PointF(_appIconRect.X, _appIconRect.Y),
            Gdiplus::PointF(_appIconRect.X + _appIconRect.Width,
                            _appIconRect.Y + _appIconRect.Height),
            Gdiplus::Color(A, 54, 192, 105),
            Gdiplus::Color(A, 120, 90, 220));
        g.FillPath(&ib, &ip);
        Gdiplus::SolidBrush fg(Gdiplus::Color(A, 255, 255, 255));
        Gdiplus::Font fallback(L"Segoe UI", 18.f * s, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::StringFormat fsf;
        fsf.SetAlignment(Gdiplus::StringAlignmentCenter);
        fsf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"F", 1, &fallback, _appIconRect, &fsf, &fg);
    }

    Gdiplus::SolidBrush titleBrush(Gdiplus::Color(A, 255, 255, 255));
    if (_titleFont) {
        Gdiplus::StringFormat tsf;
        tsf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        tsf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        tsf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        tsf.SetAlignment(Gdiplus::StringAlignmentNear);
        g.DrawString(L"FanFolder", 9, _titleFont, _titleRect, &tsf, &titleBrush);
    }

    // Close button: a quiet pill that brightens on hover.
    {
        Gdiplus::GraphicsPath cp;
        DrawRoundedRect(cp, _closeRect, 8.f * s);
        Gdiplus::SolidBrush cf(
            Gdiplus::Color((BYTE)(A * (_hoverClose ? 24 : 10) / 255), 255, 255, 255));
        g.FillPath(&cf, &cp);
        Gdiplus::Pen cpen(Gdiplus::Color((BYTE)(A * 26 / 255), 255, 255, 255), 1.f);
        g.DrawPath(&cpen, &cp);

        Gdiplus::Pen xpen(Gdiplus::Color((BYTE)(A * 205 / 255), 185, 187, 202), 1.8f * s);
        xpen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        const float cx = _closeRect.X + _closeRect.Width / 2.f;
        const float cy = _closeRect.Y + _closeRect.Height / 2.f;
        const float r = 5.f * s;
        g.DrawLine(&xpen, cx - r, cy - r, cx + r, cy + r);
        g.DrawLine(&xpen, cx + r, cy - r, cx - r, cy + r);
    }

    // Hairline divider under the header.
    {
        Gdiplus::Pen div(Gdiplus::Color((BYTE)(A * 18 / 255), 255, 255, 255), 1.f);
        g.DrawLine(&div, x0, _dividerY, x0 + _bodyRect.Width, _dividerY);
    }

    // About text.
    Gdiplus::SolidBrush bodyBrush(Gdiplus::Color((BYTE)(A * 248 / 255), 236, 237, 243));
    if (_bodyFont) {
        Gdiplus::StringFormat bsf;
        bsf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        bsf.SetLineAlignment(Gdiplus::StringAlignmentNear);
        bsf.SetAlignment(Gdiplus::StringAlignmentNear);
        g.DrawString(is.aboutText, -1, _bodyFont, _bodyRect, &bsf, &bodyBrush);
    }

    // Link rows: full-width cards, a tinted glyph tile, label, and chevron.
    static const LinkGlyph glyphs[kLinkCount] = { LinkGlyph::Heart, LinkGlyph::Home, LinkGlyph::Bug };
    for (int i = 0; i < kLinkCount; i++) {
        const Gdiplus::RectF& row = _linkRects[i];
        const bool hover = (i == _hoverLink);

        Gdiplus::GraphicsPath rp;
        DrawRoundedRect(rp, row, 10.f * s);
        Gdiplus::SolidBrush rb(
            Gdiplus::Color((BYTE)(A * (hover ? 24 : 12) / 255), 255, 255, 255));
        g.FillPath(&rb, &rp);
        Gdiplus::Pen rpen(
            Gdiplus::Color((BYTE)(A * (hover ? 42 : 26) / 255), 255, 255, 255), 1.f);
        g.DrawPath(&rpen, &rp);

        Gdiplus::Color accent;   // mint / sky blue / violet, tracing the site's brand gradient
        switch (i) {
        case 0:  accent = Gdiplus::Color(A, 110, 224, 150); break;
        case 1:  accent = Gdiplus::Color(A, 120, 175, 255); break;
        default: accent = Gdiplus::Color(A, 205, 170, 255); break;
        }

        Gdiplus::GraphicsPath tp;
        DrawRoundedRect(tp, _iconRects[i], 8.f * s);
        Gdiplus::SolidBrush tb(Gdiplus::Color(
            (BYTE)(A * (hover ? 54 : 38) / 255),
            accent.GetR(), accent.GetG(), accent.GetB()));
        g.FillPath(&tb, &tp);
        DrawGlyph(g, glyphs[i], _iconRects[i], accent, s);

        if (_linkFont) {
            Gdiplus::SolidBrush linkBrush(Gdiplus::Color(A, 232, 232, 240));
            Gdiplus::StringFormat lsf;
            lsf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
            lsf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            lsf.SetAlignment(Gdiplus::StringAlignmentNear);
            g.DrawString(_linkLabels[i].c_str(), -1, _linkFont, _linkTextRects[i], &lsf, &linkBrush);
        }

        const float chx = row.X + row.Width - 24.f * s;
        const float chy = row.Y + row.Height / 2.f;
        Gdiplus::Pen chev(
            Gdiplus::Color((BYTE)(A * (hover ? 235 : 160) / 255), 170, 173, 188), 1.6f * s);
        chev.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
        g.DrawLine(&chev, chx - 4.f * s, chy - 6.f * s, chx + 2.f * s, chy);
        g.DrawLine(&chev, chx + 2.f * s, chy, chx - 4.f * s, chy + 6.f * s);
    }

    // Dismiss hint, centered across the card
    Gdiplus::SolidBrush hintBrush(Gdiplus::Color((BYTE)(A * 190 / 255), 168, 170, 184));
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

bool InfoScreen::IsPointOnClose(int x, int y) const {
    const Gdiplus::RectF& r = _closeRect;
    return x >= r.X && x < r.X + r.Width && y >= r.Y && y < r.Y + r.Height;
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
        const int mx = GET_X_LPARAM(lParam);
        const int my = GET_Y_LPARAM(lParam);
        const bool closeHover = self->IsPointOnClose(mx, my);
        const int hit = closeHover ? -1 : self->HitTest(mx, my);
        if (closeHover != self->_hoverClose || hit != self->_hoverLink) {
            self->_hoverClose = closeHover;
            self->_hoverLink = hit;
            SetCursor(LoadCursor(nullptr, (closeHover || hit >= 0) ? IDC_HAND : IDC_ARROW));
            self->Draw();
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        if (self->_hoverLink != -1 || self->_hoverClose) {
            self->_hoverLink = -1;
            self->_hoverClose = false;
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            self->Draw();
        }
        return 0;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        if (self->IsPointOnClose(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
            self->_downLink = -1;
        else
            self->_downLink = self->HitTest(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        const int mx = GET_X_LPARAM(lParam);
        const int my = GET_Y_LPARAM(lParam);
        self->_downLink = -1;
        if (self->IsPointOnClose(mx, my)) {
            // Same dismiss path as an outside click so hook teardown stays
            // in MainWindow::CloseInfoScreen.
            if (self->_hwndParent)
                PostMessageW(self->_hwndParent, WM_INFO_DISMISSED, 0, 0);
            return 0;
        }
        int hit = self->HitTest(mx, my);
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
