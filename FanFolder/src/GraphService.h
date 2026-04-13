// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#pragma once
#include "pch.h"
#include "FileService.h"

// ---------------------------------------------------------------------------
// GraphService — Microsoft Graph API integration
//
// Retrieves recently used files from /me/insights/used using delegated auth
// (device code flow on first use, refresh token on subsequent calls).
// Tokens are stored in Windows Credential Manager under "FanFolder/Graph".
// ---------------------------------------------------------------------------
class GraphService {
public:
    // Returns recently used files from Microsoft Graph.
    // hwnd is used as parent for the sign-in dialog on first use.
    static std::vector<FileItem> GetRecentItems(HWND hwnd, int maxItems);

    // Clears stored credentials (forces re-authentication on next call).
    static void SignOut();
};
