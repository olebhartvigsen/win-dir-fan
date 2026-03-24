# Fan Folder – Copilot Instructions

## Project Overview

**Fan Folder** is a C# / .NET 8 / WinForms desktop app that replicates the macOS Dock "Fan" folder on the Windows taskbar. Clicking a taskbar icon reveals an animated, arc-shaped popup showing the 15 most recently modified items in a configured folder. Items can be opened or dragged to other applications.

- Single project, self-contained `.exe`, targets `net8.0-windows` (Windows 10+, x64 only)
- No unit tests; no linting/formatting tooling configured
- Spec document: `prompt/opret.md`

---

## Build Commands

```powershell
dotnet build                  # Debug build
dotnet build -c Release       # Release build
dotnet run                    # Run locally
dotnet publish -c Release     # Publish single-file exe to bin/Release/net8.0-windows/win-x64/publish/

# Create installer (requires Inno Setup 6)
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\setup.iss

# PowerShell installer
powershell -ExecutionPolicy Bypass -File installer\Install.ps1 -AddToStartup
powershell -ExecutionPolicy Bypass -File installer\Install.ps1 -Uninstall
```

---

## Architecture

Three layers, six source files in `FanFolderApp/`:

| File | Role |
|---|---|
| `Program.cs` | Entry point: DPI setup, config load, form launch |
| `MainHiddenForm.cs` | Invisible taskbar-resident form; intercepts clicks via `WndProc`; manages fan lifecycle |
| `FanForm.cs` | Fan popup: arc layout, rendering, animations, hover, drag-and-drop |
| `FileService.cs` | Folder scan (top 15 by `LastWriteTime`), shell icon extraction |
| `NativeMethods.cs` | All P/Invoke: Shell32, User32, Dwmapi, OLE32 |
| `ShellDragHelper.cs` | OLE drag-and-drop using shell data objects (PIDL-based) |

### Key flows

**Taskbar click → fan toggle:**
`WM_SYSCOMMAND (SC_RESTORE)` intercepted in `MainHiddenForm.WndProc` → `ToggleFan()` → creates/closes `FanForm`, switches taskbar icon between stack and arrow states.

**Fan rendering:**
`FanForm.OnPaint()` draws each item at a parametric arc position (angle + distance from taskbar edge), applies per-item `_animProgress[i]` easing, and renders shell icons with text labels.

**Icon extraction fallback chain:**
`SHIL_JUMBO (256px)` → `SHIL_EXTRALARGE (48px)` → `SHIL_LARGE (32px)` → `Icon.ExtractAssociatedIcon()`

---

## Key Conventions

**Transparency:** Both forms use `TransparencyKey = Color.Magenta` / `BackColor = Color.Magenta` for transparent windows (avoids OS-level transparency issues with `DoubleBuffered` rendering).

**Class declarations:** `internal sealed class` for all types. UI forms inherit `Form`; services and helpers do not.

**Naming:**
- Private fields: `_camelCase`
- Constants: `PascalCase` (e.g., `MaxItems`, `FadeDurationMs`)
- Section separators in long files: `// ─── Section Name ──────────`

**File-scoped namespaces:** `namespace FanFolderApp;` (no braces) in every file.

**Configuration:** `appsettings.json` is published alongside the `.exe` (not embedded). Key: `FanFolderPath`. Falls back to Desktop if path is missing or invalid.

**Arc geometry constants** (in `FanForm.cs`): `StartDistance`, `DistanceStep`, `MaxArcSpreadDeg`, `ArcSpreadPerItem` — change these to adjust the fan shape.

**Animation constants** (in `FanForm.cs`): `FadeDurationMs = 200f`, `AnimSpeed = 0.53f`, `HoverScaleMax = 1.4f`, `SlideDistance = 24`.

**`AllowUnsafeBlocks`** is enabled — needed for P/Invoke marshaling and COM interop; use only in `NativeMethods.cs` and `ShellDragHelper.cs`.

**Implicit usings** are enabled (`net8.0-windows`); common namespaces including `System.Windows.Forms` are auto-imported.
