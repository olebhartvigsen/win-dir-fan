#pragma once
#include "pch.h"
#include "FileService.h"
#include "Config.h"

class FanDropTarget;   // forward declaration for friend access

class FanWindow {
    friend class FanDropTarget;
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
    bool IsDragging() const { return _dragging; }
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

    // Icons — raw handles (ownership) + one-time-cached GDI+ bitmaps (no per-frame alloc)
    std::vector<HBITMAP>          _bitmaps;
    std::vector<HICON>            _icons;
    std::vector<bool>             _iconLoaded;
    std::vector<Gdiplus::Bitmap*> _gdiBitmaps;  // lazily filled; nullptr = not yet converted
    std::mutex                    _iconMutex;
    Gdiplus::ImageAttributes*     _drawIA = nullptr;  // reused per draw call

    // Cached backbuffer— recreated only when window size changes
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
    DWORD _createTick = 0;
    int _arcOriginX = 0;
    int _arcOriginY = 0;

    // Cached GDI+ font/format objects — rebuilt only when _iconSize changes
    float                  _cachedFontSize = 0.f;
    Gdiplus::Font*         _labelFont      = nullptr;
    Gdiplus::StringFormat* _labelSF        = nullptr;  // for drawing: Far+Center+NoWrap
    Gdiplus::StringFormat* _measureSF      = nullptr;  // for measuring: NoWrap only

    std::vector<std::wstring> _labelCache;  // cached ItemLabel() results

    // Drag (outbound)
    POINT _dragStart = {};
    int   _dragIdx   = -1;
    bool  _dragging  = false;

    // Drop target (inbound)
    bool         _dropHovering = false;
    IDropTarget* _dropTarget   = nullptr;
    void OnDropHover(bool hovering);
    void HandleFileDrop(IDataObject* pDataObj);

    // Cached taskbar button center from last real click — reused for Alt+Tab
    static int s_lastTaskbarAnchorX;

    void RebuildFontCache();
    void RebuildLabelCache();
    void CalculateLayout();
    static int FindTaskbarButtonCenter(RECT taskbarRect);
    void DrawToLayeredWindow();
    void DrawItem(Gdiplus::Graphics& g, int idx, float itemAlpha);
    std::wstring ItemLabel(int idx) const;
    void DrawLabelPill(Gdiplus::Graphics& g, float x, float y, float w, float h, float radius,
                       const std::wstring& text, float alpha);
    void DrawArrowItem(Gdiplus::Graphics& g, float cx, float cy, float sz, float alpha);
    static void DrawCachedBitmapIA(Gdiplus::Graphics& g, Gdiplus::Bitmap* bmp,
                                   float x, float y, float size, Gdiplus::ImageAttributes* ia);
    static Gdiplus::Bitmap* HBitmapToGdiBitmap(HBITMAP hBmp);
    void PremultiplyBitmap(Gdiplus::BitmapData& data);
    int  HitTest(int x, int y) const;
    void LaunchItem(int idx);
    void ShowContextMenu(int idx, POINT screenPt);
    void ShowSettingsMenu(POINT screenPt);
    void StartIconLoad(int idx);
    void FreeBackBuffer();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static FanWindow* FromHWND(HWND hwnd);
};
