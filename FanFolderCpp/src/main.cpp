#include "pch.h"
#include "Config.h"
#include "MainWindow.h"
#include "FanWindow.h"
#include <gdiplus.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

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

    CoUninitialize();
    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}
