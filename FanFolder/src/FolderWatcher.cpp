// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "FolderWatcher.h"

FolderWatcher::~FolderWatcher() {
    Stop();
}

void FolderWatcher::Start(const std::wstring& path, bool watchSubtree,
                          HWND notifyWnd, UINT msg) {
    Stop();

    if (path.empty() || path.rfind(L"::", 0) == 0 || !notifyWnd || !msg)
        return;

    // CreateFileW is happiest without a trailing separator (except for a drive
    // root such as C:\).  Normalise once here so the watch and the scan agree.
    std::wstring watchPath = path;
    while (watchPath.size() > 3 &&
           (watchPath.back() == L'\\' || watchPath.back() == L'/')) {
        watchPath.pop_back();
    }

    // FILE_FLAG_BACKUP_SEMANTICS lets CreateFileW open a directory handle.
    // FILE_FLAG_OVERLAPPED makes ReadDirectoryChangesW asynchronous, so the
    // worker can wait on the stop event at the same time as the change event.
    _dir = CreateFileW(watchPath.c_str(), FILE_LIST_DIRECTORY,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr, OPEN_EXISTING,
                       FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                       nullptr);
    if (_dir == INVALID_HANDLE_VALUE)
        return;

    _stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    _ioEvent   = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!_stopEvent || !_ioEvent) {
        Stop();
        return;
    }

    _notifyWnd    = notifyWnd;
    _msg          = msg;
    _watchSubtree = watchSubtree;
    _running.store(true, std::memory_order_release);
    try {
        _thread = std::thread(&FolderWatcher::Run, this);
    } catch (...) {
        Stop();   // creation failed: release the handle and events again
    }
}

void FolderWatcher::Stop() {
    if (_thread.joinable()) {
        _running.store(false, std::memory_order_release);
        if (_stopEvent)
            SetEvent(_stopEvent);
        _thread.join();
    } else {
        _running.store(false, std::memory_order_release);
    }

    if (_dir != INVALID_HANDLE_VALUE) {
        CloseHandle(_dir);
        _dir = INVALID_HANDLE_VALUE;
    }
    if (_stopEvent) {
        CloseHandle(_stopEvent);
        _stopEvent = nullptr;
    }
    if (_ioEvent) {
        CloseHandle(_ioEvent);
        _ioEvent = nullptr;
    }
    _notifyWnd    = nullptr;
    _msg          = 0;
    _watchSubtree = false;
}

void FolderWatcher::Run() {
    constexpr DWORD kBufferBytes = 64 * 1024;
    std::vector<BYTE> buffer(kBufferBytes);

    OVERLAPPED ov = {};
    ov.hEvent = _ioEvent;

    while (_running.load(std::memory_order_acquire)) {
        ResetEvent(_ioEvent);
        ZeroMemory(&ov, sizeof(ov));
        ov.hEvent = _ioEvent;

        DWORD syncBytes = 0;
        const BOOL started = ReadDirectoryChangesW(
            _dir, buffer.data(), kBufferBytes, _watchSubtree ? TRUE : FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
            &syncBytes, &ov, nullptr);

        if (!started && GetLastError() != ERROR_IO_PENDING)
            break;

        HANDLE waits[2] = { _stopEvent, _ioEvent };
        const DWORD wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);

        if (wait == WAIT_OBJECT_0) {
            // Stop requested while the read was pending.  Cancel it and wait
            // for the completion so the OVERLAPPED is no longer in use when
            // the caller closes the directory handle.
            CancelIoEx(_dir, &ov);
            DWORD ignored = 0;
            GetOverlappedResult(_dir, &ov, &ignored, TRUE);
            break;
        }
        if (wait != WAIT_OBJECT_0 + 1)
            break;

        DWORD bytes = 0;
        if (!GetOverlappedResult(_dir, &ov, &bytes, FALSE))
            break;

        // bytes == 0 means the notification buffer overflowed and changes were
        // lost.  A rescan is exactly what we want in that case, so post anyway.
        PostMessageW(_notifyWnd, _msg, 0, 0);
    }

    // The worker can also exit on a directory error (folder deleted, share
    // disconnected).  IsWatching() must reflect that so the owner can restart.
    _running.store(false, std::memory_order_release);
}