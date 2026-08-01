# FanFolder

**Your latest files, one click away from the Windows taskbar.**

Stop digging through File Explorer for files you just used. FanFolder puts your recent documents or any folder you choose directly on the taskbar. Click its icon to open an animated fan of files, then open, drag, copy, rename, or delete them using familiar Windows shell actions.

Use it for recent documents, an active project folder, Downloads, or any folder you open often.

FanFolder is a small native Win32/C++ application. It has no .NET runtime dependency and the executable is about 161 KB.

---

## License

FanFolder is proprietary software, free to use for personal and commercial use forever. The source code is not publicly distributed. See [LICENSE](LICENSE) for full terms.

If you find FanFolder useful, an optional "buy me a coffee" donation is appreciated but never required.

---

## Features

- Animated arc/fan popup anchored to the taskbar
- Multiple animation styles: Fan, Glide, Spring, Fade, None
- Adaptive icon size with shell icons and file thumbnails
- Configurable folder, sort order, item count, and filename filter
- Sort by date modified, date created, or name
- Shell context menu (open, copy, delete, rename, …) on right-click
- Drag-and-drop items out of the menu to other applications
- Drop files from Explorer onto the fan menu to move them into the watched folder
- Show or hide file extensions
- All settings stored in the Windows registry — accessible via the tray icon menu

---

## Requirements

- Windows 10 or 11 (x64)
- No runtime dependencies

---

## Installation

### MSI Installer (recommended)

Build and package in one step (requires Visual Studio Build Tools 2022 and WiX 4):

```powershell
.\installer\Build-Installer.ps1
# Output: installer\output\FanFolderSetup.msi
```

### Manual install

```powershell
.\installer\Install.ps1 -AddToStartup
```

### Run directly

```powershell
.\FanFolder\build\Release\FanFolder.exe
```

---

## Building from source

Requires Visual Studio 2022 (or Build Tools) with the C++ workload.

```powershell
# Configure (first time only)
cmake -B FanFolder\build -G "Visual Studio 17 2022" -A x64 -S FanFolder

# Build
cmake --build FanFolder\build --config Release
```

Output: `FanFolder\build\Release\FanFolder.exe`

---

## Anonymous installation telemetry

After FanFolder has successfully created its main window, it sends one anonymous
`app_installed` event to the FanFolder Aptabase project in the EU. The request uses
the Windows WinHTTP API, runs in the background, has a short timeout, and is
ignored when the network is unavailable. It never affects normal operation. This
telemetry is sent by both release and debug builds unless disabled in the registry.

The event contains a random per-user installation identifier, application version,
CPU architecture, and the platform identifier `Windows`. Aptabase may also derive
coarse region information from the network request. FanFolder does not send
usernames, folder paths, filenames, account information, or file activity. Aptabase
can receive normal network metadata associated with the HTTPS request. The
identifier is stored under `HKEY_CURRENT_USER\SOFTWARE\FanFolder`; the event is
marked as sent only after a successful response, so failed delivery is retried on a
later launch.

To opt out before launching FanFolder, run:

```powershell
New-Item -Path "HKCU:\SOFTWARE\FanFolder" -Force | Out-Null
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "TelemetryEnabled" -Type DWord -Value 0
```

To re-enable it:

```powershell
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "TelemetryEnabled" -Type DWord -Value 1
```

---

## Settings

All settings live under `HKEY_CURRENT_USER\SOFTWARE\FanFolder` and can be changed via the **tray icon right-click menu** or directly in the registry.

### FolderPath

**Type:** `REG_SZ` | **Default:** `::RecentDocs::`

The folder whose contents are displayed in the fan menu. In addition to a normal
filesystem path, three special virtual values are supported:

| Value | Shows |
|---|---|
| `::RecentDocs::` | Recently used documents aggregated from the Windows Jump Lists **(default)** |
| `::RecentFiles::` | Explorer's "Recent" list (`%APPDATA%\Microsoft\Windows\Recent`) |
| `::GraphRecent::` | Recent Office/M365 documents only, ordered by last-opened time |

```powershell
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "FolderPath" -Value "C:\Users\You\Documents"
```

---

### SortMode

**Type:** `REG_SZ` | **Default:** `DateModifiedDesc`

| Value | Description |
|---|---|
| `DateModifiedDesc` | Most recently modified first **(default)** |
| `DateModifiedAsc` | Oldest modification first |
| `DateCreatedDesc` | Most recently created first |
| `DateCreatedAsc` | Oldest creation first |
| `NameAsc` | File name A → Z |
| `NameDesc` | File name Z → A |

```powershell
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "SortMode" -Value "DateModifiedDesc"
```

---

### MaxItems

**Type:** `REG_DWORD` | **Default:** `15` | **Range:** 1 – 50 (tray menu offers 5 / 10 / 15 / 20 / 25)

Maximum number of items shown. The tray menu selects it in steps of 5; any value
from 1 to 50 can be set directly in the registry.

```powershell
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "MaxItems" -Value 10
```

---

### IncludeDirectories

**Type:** `REG_DWORD` | **Default:** `1`

Whether sub-folders appear alongside files. `1` = include, `0` = files only.

```powershell
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "IncludeDirectories" -Value 0
```

---

### ShowExtensions

**Type:** `REG_DWORD` | **Default:** `0`

Whether file extensions are shown in item labels. `1` = show, `0` = hide.

```powershell
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "ShowExtensions" -Value 1
```

---

### FilterRegex

**Type:** `REG_SZ` | **Default:** *(empty — no filter)*

A regular expression matched case-insensitively against each item's filename. Only matching items are shown.

```powershell
# Show only PDFs and Word documents
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "FilterRegex" -Value "\.(pdf|docx?)$"

# Remove filter
Remove-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "FilterRegex" -ErrorAction SilentlyContinue
```

---

### AnimationStyle

**Type:** `REG_SZ` | **Default:** `Glide`

| Value | Effect |
|-------|--------|
| `Fan` | Items fly in one-by-one from the arc hinge, staggered (ease-out-quart). |
| `Glide` | All items drift upward while fading in together (ease-out-cubic). |
| `Spring` | Items scale in from zero with a slight overshoot bounce, staggered. |
| `Fade` | Instant layout, very short fade-in only. |
| `None` | Instant — no animation. |

```powershell
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "AnimationStyle" -Value "Spring"
```

---

## Uninstall

```powershell
.\installer\Install.ps1 -Uninstall

# Remove registry settings
Remove-Item -Path "HKCU:\SOFTWARE\FanFolder" -Recurse -ErrorAction SilentlyContinue
```

