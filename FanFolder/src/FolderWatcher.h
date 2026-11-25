// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#pragma once
#include "pch.h"

// Watches a directory with ReadDirectoryChangesW and posts a notification to
// the owner window when an entry is created, renamed, or deleted.  The watch
// runs on its own thread.  Start() and Stop() are called from the UI thread.
//
// The change details are queued, not posted: the notification is still a plain
// PostMessageW, and the owner drains the queue with TakeChanges(). That lets the
// search index patch one directory instead of rebuilding the whole tree.
class FolderWatcher {
public:
    struct Change {
        std::wstring relPath;   // relative to the watched root, "Sub\\file.txt"
        bool         removed;   // deleted, or renamed away
    };

    FolderWatcher() = default;
    ~FolderWatcher();

    FolderWatcher(const FolderWatcher&) = delete;
    FolderWatcher& operator=(const FolderWatcher&) = delete;

    // Replaces any existing watch.  Virtual sentinels (paths starting with
    // "::") and missing folders are ignored; IsWatching() then stays false.
    void Start(const std::wstring& path, bool watchSubtree, HWND notifyWnd, UINT msg);
    void Stop();

    bool IsWatching() const { return _running.load(std::memory_order_acquire); }

    // Moves the queued changes to `out`. Returns false when the notification
    // buffer overflowed, which means changes were lost and the caller has to
    // rebuild from scratch instead of patching.
    bool TakeChanges(std::vector<Change>& out);

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

    std::mutex          _changesMutex;
    std::vector<Change> _changes;
    bool                _overflow = false;
};
