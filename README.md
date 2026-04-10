# Fan Folder

A Windows taskbar app that replicates the macOS Dock **fan folder** — click the taskbar icon to reveal an animated arc menu showing the most recently modified files in a configured folder. Items can be opened, dragged, renamed, and managed via the standard Windows shell context menu.

---

## Features

- Animated arc/fan popup anchored to the taskbar
- Adaptive icon size (shell icons, thumbnails)
- Configurable folder, sort order, item count, and filters
- Shell context menu (open, copy, delete, rename, …)
- Drag-and-drop from the fan to other applications
- Alt+Tab support — the menu opens when you switch to the app
- All settings stored in the Windows registry (no config files)

---

## Requirements

- Windows 10 or 11 (x64)
- .NET 8 runtime (included in the self-contained publish)

---

## Installation

### Installer (recommended)

Run `installer\setup.iss` with [Inno Setup 6](https://jrsoftware.org/isinfo.php):

```powershell
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\setup.iss
```

Or use the PowerShell installer:

```powershell
powershell -ExecutionPolicy Bypass -File installer\Install.ps1 -AddToStartup
```

### Manual

Publish a self-contained executable and run it:

```powershell
dotnet publish -c Release
# Output: FanFolderApp\bin\Release\net8.0-windows\win-x64\publish\FanFolderApp.exe
```

---

## Registry Settings

All settings live under:

```
HKEY_CURRENT_USER\SOFTWARE\FanFolder
```

You can edit them with **regedit**, the PowerShell snippets below, or any registry editor.

---

### FolderPath

**Type:** `REG_SZ`  
**Default:** `%USERPROFILE%\Downloads`

The folder whose contents are displayed in the fan menu.

```powershell
# Set to a custom folder
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "FolderPath" -Value "C:\Users\You\Documents"

# Set to Downloads (default)
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "FolderPath" -Value "$env:USERPROFILE\Downloads"
```

---

### MaxItems

**Type:** `REG_DWORD`  
**Default:** `15`  
**Range:** 1 – 50

Maximum number of items displayed in the fan menu. The menu height scales proportionally.

```powershell
# Show 10 items
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "MaxItems" -Value 10

# Show 25 items
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "MaxItems" -Value 25
```

---

### SortMode

**Type:** `REG_SZ`  
**Default:** `DateModifiedDesc`

Controls how items are ordered in the menu. The value is case-insensitive.

| Value | Description |
|---|---|
| `DateModifiedDesc` | Most recently modified first **(default)** |
| `DateModifiedAsc` | Oldest modification first |
| `DateCreatedDesc` | Most recently created first |
| `DateCreatedAsc` | Oldest creation first |
| `NameAsc` | File name A → Z |
| `NameDesc` | File name Z → A |
| `SizeDesc` | Largest files first |
| `SizeAsc` | Smallest files first |

```powershell
# Most recently modified first (default)
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "SortMode" -Value "DateModifiedDesc"

# Alphabetical A → Z
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "SortMode" -Value "NameAsc"

# Largest files first
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "SortMode" -Value "SizeDesc"
```

---

### IncludeDirectories

**Type:** `REG_DWORD`  
**Default:** `1` (true)

Whether sub-folders inside the watched folder appear in the menu alongside files.

```powershell
# Include directories (default)
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "IncludeDirectories" -Value 1

# Files only — hide all sub-folders
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "IncludeDirectories" -Value 0
```

---

### FilterRegex

**Type:** `REG_SZ`  
**Default:** *(empty — no filter)*

A .NET regular expression evaluated against the **full path** of each item. Only items whose path matches the pattern are shown. Leave empty (or delete the value) to show everything.

The match is **case-insensitive**.

```powershell
# Show only PDF and Word documents
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "FilterRegex" -Value "\.(pdf|docx?)$"

# Show only files in a specific sub-folder
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "FilterRegex" -Value "\\Projects\\"

# Exclude temporary files (files starting with ~ or ending in .tmp)
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "FilterRegex" -Value "^(?!.*[/\\]~)(?!.*\.tmp$)"

# Remove filter (show everything)
Remove-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "FilterRegex" -ErrorAction SilentlyContinue
```

---

## Apply changes

Registry settings are read at startup. Restart the app after making changes:

```powershell
# Restart Fan Folder
$p = Get-Process | Where-Object { $_.Name -like "*FanFolder*" }
if ($p) { Stop-Process -Id $p.Id -Force }
Start-Sleep -Seconds 1
Start-Process "C:\path\to\FanFolderApp.exe"
```

---

## Building from source

```powershell
dotnet build                  # Debug
dotnet build -c Release       # Release
dotnet run                    # Run locally
dotnet publish -c Release     # Self-contained single-file exe
```

---

## Uninstall

```powershell
powershell -ExecutionPolicy Bypass -File installer\Install.ps1 -Uninstall

# Remove registry settings
Remove-Item -Path "HKCU:\SOFTWARE\FanFolder" -Recurse -ErrorAction SilentlyContinue
```
