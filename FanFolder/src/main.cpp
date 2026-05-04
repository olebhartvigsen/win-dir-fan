// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "Config.h"
#include "MainWindow.h"
#include "FanWindow.h"
#include <gdiplus.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Single-instance guard. A named mutex in the Local\ namespace ensures only
    // one FanFolder.exe runs per user session; if a second copy launches (e.g.
    // from an installer upgrade or a stray shortcut) we surface the existing
    // window and exit so the taskbar stays on a single live preview.
    HANDLE hInstanceMutex = CreateMutexW(nullptr, FALSE, L"Local\\FanFolder.App.SingleInstance");
    if (hInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        // Ask any existing hidden FanFolder top-level window to come forward.
        HWND existing = FindWindowW(L"FanFolderMain", nullptr);
        if (existing) {
            // Broadcast-safe restore: the main window's SC_RESTORE handler
            // toggles the fan, so we simply show it minimized again to bring
            // the taskbar button back if it was hidden.
            ShowWindow(existing, SW_SHOWMINNOACTIVE);
            SetForegroundWindow(existing);
        }
        if (hInstanceMutex) CloseHandle(hInstanceMutex);
        return 0;
    }

    // Set explicit AppUserModelID so the taskbar shows "FanFolder" in the taskbar
    SetCurrentProcessExplicitAppUserModelID(L"FanFolder.App");

    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);

    // Force GDI+ to fully initialise its internal caches (font enumeration,
    // codec registration, codec instantiation, thread-local state).  Without
    // this, the very first user-visible render can stall or produce a blank
    // frame in sandboxed or headless environments.  We exercise diverse drawing
    // operations to warm multiple code paths, not just basic bitmap creation.
    {
        Gdiplus::Bitmap warmBmp(32, 32, PixelFormat32bppARGB);
        Gdiplus::Graphics warmG(&warmBmp);
        warmG.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

        // Solid brushes and pens
        Gdiplus::SolidBrush brBlack(Gdiplus::Color(255, 0, 0, 0));
        Gdiplus::SolidBrush brWhite(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::SolidBrush brBlue(Gdiplus::Color(255, 0, 0, 255));
        Gdiplus::Pen penRed(Gdiplus::Color(255, 255, 0, 0), 2.0f);

        // Draw rectangles (fill + stroke)
        warmG.FillRectangle(&brBlack, 0, 0, 32, 32);
        warmG.FillRectangle(&brBlue, 4, 4, 24, 24);
        warmG.DrawRectangle(&penRed, 2, 2, 28, 28);

        // Draw circles/arcs (exercise bezier curves, antialiasing)
        warmG.DrawEllipse(&penRed, 8, 8, 16, 16);
        warmG.FillEllipse(&brWhite, 12, 12, 8, 8);

        // Draw lines (exercise pen rendering)
        warmG.DrawLine(&penRed, 0, 16, 32, 16);
        warmG.DrawLine(&penRed, 16, 0, 16, 32);

        // Font rendering (exercising text rendering path and font cache)
        Gdiplus::FontFamily fontFamily(L"Arial");
        Gdiplus::Font font(&fontFamily, 12.0f);
        Gdiplus::StringFormat strFormat;
        strFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
        strFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::RectF textRect(0.0f, 0.0f, 32.0f, 32.0f);
        warmG.DrawString(L"A", 1, &font, textRect, &strFormat, &brWhite);
    }

    // OleInitialize initializes COM *and* the OLE drag-drop/clipboard subsystem.
    // CoInitializeEx alone is not sufficient — DoDragDrop requires OleInitialize.
    OleInitialize(nullptr);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    ConfigData config = Config::Load();

    MainWindow::Register(hInstance);
    FanWindow::Register(hInstance);

    MainWindow mainWnd(hInstance, config);
    if (!mainWnd.Create()) return 1;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    OleUninitialize();
    Gdiplus::GdiplusShutdown(gdiplusToken);
    if (hInstanceMutex) CloseHandle(hInstanceMutex);
    return 0;
}
