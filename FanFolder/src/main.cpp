#include "pch.h"
#include "Config.h"
#include "MainWindow.h"
#include "FanWindow.h"
#include <gdiplus.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);

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
    return 0;
}
