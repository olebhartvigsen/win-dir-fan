# FanFolder

A Windows taskbar app that replicates the macOS Dock **fan folder** — click the taskbar icon to reveal an animated arc menu showing the most recently modified files in a configured folder. Items can be opened, dragged, and managed via the standard Windows shell context menu.

Native Win32/C++ application. No .NET runtime required. ~161 KB executable.

---

## License

- Source code in this repository is licensed under the MIT License. See [LICENSE](LICENSE).
- Use of the compiled application is governed by the End User License Agreement. See [EULA.md](EULA.md).
- FanFolder is free to use. If you like it, optional support via a "buy me a coffee" payment is appreciated.

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

## Settings

All settings live under `HKEY_CURRENT_USER\SOFTWARE\FanFolder` and can be changed via the **tray icon right-click menu** or directly in the registry.

### FolderPath

**Type:** `REG_SZ` | **Default:** `%USERPROFILE%\Downloads`

The folder whose contents are displayed in the fan menu.

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

**Type:** `REG_DWORD` | **Default:** `15` | **Range:** 5 – 25

Maximum number of items shown. Selectable in steps of 5 via the tray menu.

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

**Type:** `REG_SZ` | **Default:** `Spring`

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

