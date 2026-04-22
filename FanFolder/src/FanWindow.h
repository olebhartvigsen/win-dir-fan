// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
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
                            std::vector<std::shared_ptr<Gdiplus::Bitmap>>&& gdiBitmaps,
                            int                    iconSize);
    HWND Handle() const { return _hwnd; }
    bool IsVisible() const;
    bool IsDragging() const { return _dragging; }
    void Reposition();

    static const wchar_t* ClassName() { return L"FanFolderFan"; }
    static void Register(HINSTANCE hInst);
    // Convert an HBITMAP into a premultiplied-alpha Gdiplus::Bitmap*.  Exposed
    // so the MainWindow prewarm worker can pre-convert on its thread, avoiding
    // ~750ms of per-open conversion work on the UI thread.  Thread-safe.
    static Gdiplus::Bitmap* HBitmapToGdiBitmap(HBITMAP hBmp);

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
    std::vector<std::shared_ptr<Gdiplus::Bitmap>> _gdiBitmaps;  // prewarm-filled or lazy on UI thread; owns lifetime
    std::mutex                    _iconMutex;
    Gdiplus::ImageAttributes*     _drawIA = nullptr;  // reused per draw call

    // Pre-rendered shadow cache — rebuilt only when hover target or scale changes
    int              _shadowIdx    = -1;
    float            _shadowHsc    = 0.f;
    Gdiplus::Bitmap* _shadowBmp    = nullptr;
    float            _shadowOffX   = 0.f;   // offset from icon origin to shadow bitmap origin
    float            _shadowOffY   = 0.f;

    // Cached backbuffer— recreated only when window size changes
    HBITMAP          _hBackDIB  = nullptr;
    void*            _pBackBits = nullptr;
    HDC              _hdcBack   = nullptr;
    Gdiplus::Bitmap* _backBmp   = nullptr;
    int              _backW     = 0;
    int              _backH     = 0;

    // Persistent tiny bitmap for text measurement (avoids per-layout allocation)
    Gdiplus::Bitmap  _measureBmp{1, 1, PixelFormat32bppARGB};

    // Pre-warmed icons injected before Show()
    std::vector<HBITMAP> _prewarmBitmaps;
    std::vector<HICON>   _prewarmIcons;
    std::vector<std::shared_ptr<Gdiplus::Bitmap>> _prewarmGdiBitmaps;
    int                  _prewarmIconSize = 0;

    // Animation
    float _entryAlpha = 0.f;
    std::vector<float> _itemProgress;
    std::vector<float> _hoverScale;
    std::vector<float> _entryProgress;
    bool  _entryDone  = false;
    int   _hoverIdx   = -1;
    bool  _hasExplorerButton = true;  // false for ::GraphRecent:: mode
    bool  _animating  = true;
    DWORD _createTick = 0;
    int _arcOriginX = 0;
    int _arcOriginY = 0;

    // Set by async icon-load handlers (WM_USER + 1 / + 2) to request a single
    // coalesced redraw on the next animation tick.  BEFORE this coalescing,
    // each of the 15 async icon loads triggered its own DrawToLayeredWindow()
    // inside the message handler — a storm of sequential GDI+ renders that
    // blocked the UI thread for ~750 ms on "cached" reopens, during which
    // WM_ANIM_TICK (and everything else) could not be processed.  The fan
    // therefore appeared to open with a ~750 ms lag on every reopen where the
    // prewarm hadn't finished yet.
    std::atomic<bool> _iconsDirty{false};

    // Animation driver — uses a threadpool timer instead of SetTimer because
    // WM_TIMER is low-priority and can be starved for 1+ second by queued
    // input/paint messages on rapid taskbar reopens.  The threadpool timer
    // PostMessage()s WM_ANIM_TICK which has normal priority.
    PTP_TIMER _animTimer = nullptr;
    void StartAnimTimer();
    void StopAnimTimer();
    static VOID CALLBACK AnimTimerCb(PTP_CALLBACK_INSTANCE, PVOID ctx, PTP_TIMER);

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
    static HWND FindTaskbarOnMonitor(HMONITOR hMon, RECT& outRect);
    static int  FindTaskbarButtonCenter(HWND hTaskbar, RECT taskbarRect);
    void DrawToLayeredWindow();
    void DrawItem(Gdiplus::Graphics& g, int idx, float itemAlpha);
    std::wstring ItemLabel(int idx) const;
    void DrawLabelPill(Gdiplus::Graphics& g, float x, float y, float w, float h, float radius,
                       const std::wstring& text, float alpha);
    void DrawArrowItem(Gdiplus::Graphics& g, float cx, float cy, float sz, float alpha);
    static void DrawCachedBitmapIA(Gdiplus::Graphics& g, Gdiplus::Bitmap* bmp,
                                   float x, float y, float size, Gdiplus::ImageAttributes* ia);
    void PremultiplyBitmap(Gdiplus::BitmapData& data);
    Gdiplus::Bitmap* RenderShadow(Gdiplus::Bitmap* srcBmp, float drawSz, float hsc);
    void InvalidateShadow();
    int  HitTest(int x, int y) const;
    void LaunchItem(int idx);
    void ShowContextMenu(int idx, POINT screenPt);
    void StartIconLoad(int idx);
    void FreeBackBuffer();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static FanWindow* FromHWND(HWND hwnd);
};
