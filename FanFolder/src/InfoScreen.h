// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#pragma once
#include "pch.h"

// First-launch information panel.  A small layered popup in the same visual
// family as the fan menu (dark rounded card, Segoe UI text, accent links).
// Shown once on the very first app start, or on every launch when the user
// enables "Show info on launch" in the tray menu.  Clicking anywhere outside
// the three link rows closes it; the links open in the default browser.
class InfoScreen {
public:
    InfoScreen(HINSTANCE hInst) : _hInst(hInst) {}
    ~InfoScreen();

    bool Create(HWND hwndParent);   // creates the hidden window, sized for DPI
    void Show();                    // centers, fades in, never takes focus
    void Close();
    bool IsVisible() const { return _hwnd && IsWindowVisible(_hwnd); }
    HWND Handle() const { return _hwnd; }

    // True when a screen point sits over the visible card (the window also has
    // a small transparent margin that must NOT count as "on the panel").
    bool IsPointOnPanel(POINT screenPt) const;

    // Posted to the owner window when the panel should be dismissed (body
    // click handled inside, or an outside click seen by the mouse hook).
    static constexpr UINT WM_INFO_DISMISSED = WM_USER + 21;

    static const wchar_t* ClassName() { return L"FanFolderInfo"; }
    static void Register(HINSTANCE hInst);

private:
    HINSTANCE _hInst;
    HWND      _hwnd      = nullptr;
    HWND      _hwndParent = nullptr;

    int _winW = 0, _winH = 0;
    int _scale = 100;                 // monitor DPI scale, percent (100 / 125 / 150 ...)
    int _fontsBuiltScale = 0;

    // DIB backbuffer for UpdateLayeredWindow (same recipe as FanWindow)
    HBITMAP           _hBackDIB  = nullptr;
    void*             _pBackBits = nullptr;
    HDC               _hdcBack   = nullptr;
    Gdiplus::Bitmap*  _backBmp   = nullptr;
    int               _backW = 0, _backH = 0;

    // Layout, all recomputed by CalculateLayout()
    Gdiplus::RectF                _cardRect;
    Gdiplus::RectF                _titleRect;
    Gdiplus::RectF                _bodyRect;
    Gdiplus::RectF                _hintRect;
    std::vector<Gdiplus::RectF>   _linkRects;   // full clickable row per link
    std::vector<Gdiplus::RectF>   _iconRects;   // glyph box per link
    std::vector<Gdiplus::RectF>   _linkTextRects;   // label layouts (centered)

    Gdiplus::Font* _titleFont = nullptr;
    Gdiplus::Font* _bodyFont  = nullptr;
    Gdiplus::Font* _linkFont  = nullptr;
    Gdiplus::Font* _hintFont  = nullptr;

    std::vector<std::wstring> _linkLabels;   // localized link texts

    int   _hoverLink = -1;
    int   _downLink  = -1;
    float _fadeAlpha = 0.f;

    // Card bounds in SCREEN coordinates, cached on Show() so the LL mouse
    // hook thread can test "is this click on the panel" without touching
    // window state cross-thread.
    RECT  _cardScreenRect = {};
    bool  _cardRectValid  = false;

    static constexpr UINT kFadeTimerId = 1;
    static constexpr int  kLinkCount   = 3;

    void CalculateLayout();
    void RebuildFonts();
    void Draw();
    int  HitTest(int x, int y) const;
    void LaunchLink(int idx);
    void EnsureBackBuffer();
    void FreeBackBuffer();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static InfoScreen* FromHWND(HWND hwnd) {
        return reinterpret_cast<InfoScreen*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
};
