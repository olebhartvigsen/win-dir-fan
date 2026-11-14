// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#pragma once
#include "pch.h"

// Watches a directory with ReadDirectoryChangesW and posts a notification to
// the owner window when an entry is created, renamed, or deleted.  The watch
// runs on its own thread.  Start() and Stop() are called from the UI thread;
// the notification is a plain PostMessageW with no payload.
class FolderWatcher {
public:
    FolderWatcher() = default;
    ~FolderWatcher();

    FolderWatcher(const FolderWatcher&) = delete;
    FolderWatcher& operator=(const FolderWatcher&) = delete;

    // Replaces any existing watch.  Virtual sentinels (paths starting with
    // "::") and missing folders are ignored; IsWatching() then stays false.
    void Start(const std::wstring& path, bool watchSubtree, HWND notifyWnd, UINT msg);
    void Stop();

    bool IsWatching() const { return _running.load(std::memory_order_acquire); }

private:
    void Run();

    std::thread       _thread;
    HANDLE            _dir          = INVALID_HANDLE_VALUE;
    HANDLE            _stopEvent    = nullptr;
    HANDLE            _ioEvent      = nullptr;
    std::atomic<bool> _running{false};
    HWND              _notifyWnd    = nullptr;
    UINT              _msg          = 0;
    bool              _watchSubtree = false;
};