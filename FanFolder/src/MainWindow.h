// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
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

    static const wchar_t* ClassName() { return L"FanFolderMain"; }
    static void Register(HINSTANCE hInst);

private:
    HINSTANCE      _hInst;
    HWND           _hwnd = nullptr;
    ConfigData     _config;
    NOTIFYICONDATAW _nid = {};     // system tray icon

    std::unique_ptr<FanWindow> _fanWindow;
    DWORD  _lastToggleTick = 0;
    DWORD  _fanOpenTick    = 0;
    bool   _fanOpen        = false;

    // Prewarm cache — file list + pre-loaded icons
    struct PrewarmData {
        std::vector<FileItem>  items;
        std::vector<HBITMAP>   bitmaps;
        std::vector<HICON>     icons;
        int                    iconSize = 0;
        bool                   ready    = false;
        int                    gen      = 0;   // matches _prewarmGen at post time

        void FreeHandles() {
            for (auto h : bitmaps) if (h) DeleteObject(h);
            for (auto h : icons)   if (h) DestroyIcon(h);
            bitmaps.clear();
            icons.clear();
            ready = false;
        }
    };
    PrewarmData          _prewarm;
    std::mutex           _prewarmMutex;
    std::atomic<int>     _prewarmGen{0};  // incremented each StartPrewarm; stale threads self-discard

    // Hook thread
    std::thread      _hookThread;
    DWORD            _hookThreadId = 0;
    HHOOK            _mouseHook    = nullptr;
    HHOOK            _kbHook       = nullptr;
    HWINEVENTHOOK    _hWinEvent    = nullptr;

    void ToggleFan();
    void OpenFan();
    void CloseFan();
    void StartPrewarm();
    void InstallHooks();
    void UninstallHooks();
    void ProvideIconicThumbnail(int w, int h);
    void AddTrayIcon();
    void RemoveTrayIcon();
    void ShowTrayMenu();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static MainWindow* FromHWND(HWND hwnd);

    static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static void CALLBACK    WinEventProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);

    // Statics for hook access
    static MainWindow* s_instance;
};
