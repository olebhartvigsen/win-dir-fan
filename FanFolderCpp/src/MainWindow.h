#pragma once
#include "pch.h"
#include "Config.h"
#include "FanWindow.h"
#include "FileService.h"

class MainWindow {
public:
    MainWindow(HINSTANCE hInst, const ConfigData& config);
    ~MainWindow();

    bool Create();
    HWND Handle() const { return _hwnd; }

    static const wchar_t* ClassName() { return L"FanFolderCppMain"; }
    static void Register(HINSTANCE hInst);

private:
    HINSTANCE      _hInst;
    HWND           _hwnd = nullptr;
    ConfigData     _config;

    std::unique_ptr<FanWindow> _fanWindow;
    DWORD  _lastToggleTick = 0;
    DWORD  _fanOpenTick    = 0;
    bool   _fanOpen        = false;

    // Prewarm cache
    std::vector<FileItem>  _prewarmItems;
    std::mutex             _prewarmMutex;
    bool                   _prewarmReady = false;

    // Hook thread
    std::thread  _hookThread;
    DWORD        _hookThreadId = 0;
    HHOOK        _mouseHook    = nullptr;
    HHOOK        _kbHook       = nullptr;

    void ToggleFan();
    void OpenFan();
    void CloseFan();
    void StartPrewarm();
    void InstallHooks();
    void UninstallHooks();
    void ProvideIconicThumbnail(int w, int h);

    HICON CreateStackIcon(int size);

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static MainWindow* FromHWND(HWND hwnd);

    static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);

    // Statics for hook access
    static MainWindow* s_instance;
};
