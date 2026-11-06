// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#pragma once
#include "pch.h"

// All UI strings used in menus and dialogs.
// Obtain the active locale's strings via GetStrings().
struct Strings {
    // Tray menu — sort submenu
    const wchar_t* sortBy;
    const wchar_t* sortDateModDesc;
    const wchar_t* sortDateModAsc;
    const wchar_t* sortDateCreatedDesc;
    const wchar_t* sortDateCreatedAsc;
    const wchar_t* sortNameAsc;
    const wchar_t* sortNameDesc;

    // Tray menu — top level
    const wchar_t* maxItems;
    const wchar_t* animation;
    const wchar_t* animFan;
    const wchar_t* animGlide;
    const wchar_t* animSpring;
    const wchar_t* animFade;
    const wchar_t* animNone;
    const wchar_t* includeFolders;
    const wchar_t* showExtensions;
    const wchar_t* changeFolder;    // includes trailing ellipsis
    const wchar_t* exitApp;

    // Folder label prefix in tray menu header ("Open: ")
    const wchar_t* openPrefix;

    // IFileDialog title for folder picker
    const wchar_t* selectFolderDlg;

    // Folder submenu
    const wchar_t* folderSubmenu;       // e.g. "Folder"
    const wchar_t* folderDownloads;
    const wchar_t* folderDesktop;
    const wchar_t* folderDocuments;
    const wchar_t* folderRecentDocs;    // "Recent files" (Windows shell:recent)
    const wchar_t* folderRecentFiles;   // "Recent Explorer files" (shell:Recent)
    const wchar_t* folderGraphRecent;   // "Recent Office 365 documents"
    const wchar_t* folderBrowse;        // "Browse…"
    const wchar_t* openInExplorer;      // "Open in Explorer" label on fan arrow button
    const wchar_t* visitWebpage;        // "Visit the webpage" tray-menu item
};

// Returns a reference to the Strings for the current Windows UI language,
// falling back to English if the language is not supported.
const Strings& GetStrings();

// Returns the localized "this folder is empty" placeholder shown in the fan
// when the configured folder / recent-documents list yields no items. Kept
// separate from the positional Strings tables so the 29 locale initializers
// don't all need editing.
const wchar_t* EmptyFolderLabel();

// Strings for the first-launch info panel (and its tray toggle).  Keyed
// struct instead of positional, so adding a locale later means adding one
// initializer, not re-aligning 29 tables.  Field order: aboutText,
// supportLink, homepageLink, reportLink, clickHint, trayItem.
struct InfoStrings {
    const wchar_t* aboutText;
    const wchar_t* supportLink;
    const wchar_t* homepageLink;
    const wchar_t* reportLink;
    const wchar_t* clickHint;
    const wchar_t* trayItem;   // tray-menu toggle: "Show info on launch"
};

// Returns the info-panel strings for the current Windows UI language,
// falling back to English if the language is not supported.
const InfoStrings& GetInfoStrings();
