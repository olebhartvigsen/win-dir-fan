#pragma once
#include "pch.h"
#include "FileService.h"
#include "Config.h"

class FanWindow {
public:
    FanWindow(HINSTANCE hInst, HWND hwndOwner, const ConfigData& config,
              std::vector<FileItem> items = {});
    ~FanWindow();

    bool Create();
    void Show();
    void Close();
    void AcceptPrewarmIcons(std::vector<HBITMAP>&& bitmaps,
                            std::vector<HICON>&&   icons,
                            int                    iconSize);
    HWND Handle() const { return _hwnd; }
    bool IsVisible() const;
    void Reposition();

    static const wchar_t* ClassName() { return L"FanFolderCppFan"; }
    static void Register(HINSTANCE hInst);

    // Message sent to hwndOwner when settings change (lParam = new ConfigData*)
    static constexpr UINT WM_SETTINGS_CHANGED = WM_USER + 10;

private:
    HINSTANCE _hInst;
    HWND      _hwndOwner;
    HWND      _hwnd = nullptr;
    ConfigData _config;
    std::vector<FileItem> _items;

    // Layout
    int   _winWidth  = 0, _winHeight  = 0;
    int   _winX      = 0, _winY       = 0;
    int   _iconSize  = 64;
    float _maxStackHeight = 600.f;

    std::vector<POINT> _iconPos;      // center positions in window coords
    std::vector<RECT>  _hitRects;
    std::vector<float> _labelWidths;
    int                _labelOffsetX = 0;

    // Icons — raw handles (ownership) + cached GDI+ bitmaps (draw-ready, no per-frame alloc)
    std::vector<HBITMAP>          _bitmaps;
    std::vector<HICON>            _icons;
    std::vector<bool>             _iconLoaded;
    std::vector<Gdiplus::Bitmap*> _gdiBitmaps;  // populated once per icon, used every frame

    // Cached backbuffer — recreated only when window size changes
    HBITMAP          _hBackDIB  = nullptr;
    void*            _pBackBits = nullptr;
    HDC              _hdcBack   = nullptr;
    Gdiplus::Bitmap* _backBmp   = nullptr;
    int              _backW     = 0;
    int              _backH     = 0;

    // Pre-warmed icons injected before Show()
    std::vector<HBITMAP> _prewarmBitmaps;
    std::vector<HICON>   _prewarmIcons;
    int                  _prewarmIconSize = 0;

    // Animation
    float _entryAlpha = 0.f;
    std::vector<float> _itemProgress;
    std::vector<float> _hoverScale;
    std::vector<float> _entryProgress;
    bool  _entryDone  = false;
    int   _hoverIdx   = -1;
    bool  _animating  = true;
    LARGE_INTEGER _createQPC  = {};  // QPC tick at fan open — sub-millisecond precision
    LARGE_INTEGER _qpcFreq    = {};  // QPC frequency
    bool  _timerPeriodActive  = false;  // tracks timeBeginPeriod(1) state
    int _arcOriginX = 0;
    int _arcOriginY = 0;

    // Drag
    POINT _dragStart = {};
    int   _dragIdx   = -1;
    bool  _dragging  = false;

    // Cached taskbar button center from last real click — reused for Alt+Tab
    static int s_lastTaskbarAnchorX;

    void CalculateLayout();
    static int FindTaskbarButtonCenter(RECT taskbarRect);
    void DrawToLayeredWindow();
    void DrawItem(Gdiplus::Graphics& g, int idx, float itemAlpha);
    void DrawLabelPill(Gdiplus::Graphics& g, float x, float y, float w, float h, float radius,
                       const std::wstring& text, float alpha);
    void DrawArrowItem(Gdiplus::Graphics& g, float cx, float cy, float sz, float alpha);
    void DrawGdiBitmapFit(Gdiplus::Graphics& g, Gdiplus::Bitmap* bmp,
                          float x, float y, float size, Gdiplus::ImageAttributes* ia);
    void PremultiplyBitmap(Gdiplus::BitmapData& data);
    int  HitTest(int x, int y) const;
    void LaunchItem(int idx);
    void ShowContextMenu(int idx, POINT screenPt);
    void ShowSettingsMenu(POINT screenPt);
    void StartIconLoad(int idx);
    void FreeBackBuffer();
    void CacheGdiBitmap(int idx);   // convert _bitmaps[idx] or _icons[idx] → _gdiBitmaps[idx]
    static Gdiplus::Bitmap* HBitmapToGdiBitmap(HBITMAP hBmp);

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static FanWindow* FromHWND(HWND hwnd);
};
