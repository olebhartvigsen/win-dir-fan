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
};

// Returns a reference to the Strings for the current Windows UI language,
// falling back to English if the language is not supported.
const Strings& GetStrings();
