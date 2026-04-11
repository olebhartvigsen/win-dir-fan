# Fan Folder – Copilot Instructions

## ⚠️ Primary Version: C++ (`FanFolderCpp/`)

**Always build, run, and improve the C++ version** (`FanFolderCpp/`) unless the user explicitly says otherwise.

The **C# version** (`FanFolderApp/`) is a feature-reference prototype only — it is **not actively developed**. Its sole purpose is to document the complete feature set that must be working in the C++ version. When in doubt about intended behavior, consult the C# source as the specification.

---

## Project Overview

**Fan Folder** is a Win32/C++ desktop app that replicates the macOS Dock "Fan" folder on the Windows taskbar. Clicking a taskbar icon reveals an animated, arc-shaped popup showing the most recently modified items in a configured folder. Items can be opened, right-clicked (full shell context menu), or dragged to other applications.

- **Primary implementation:** `FanFolderCpp/` — C++20, Win32, GDI+, CMake, MSVC, ~161 KB exe
- **Reference prototype:** `FanFolderApp/` — C# / .NET 8 / WinForms (do not modify or run unless asked)
- No unit tests; no linting/formatting tooling configured
- Spec document: `prompt/opret.md`

---

## Build Commands (C++ — primary)

```powershell
# Build (requires Visual Studio 2022 with C++ workload)
cmake --build FanFolderCpp\build --config Release

# First-time cmake configure (if build\ doesn't exist)
cmake -B FanFolderCpp\build -G "Visual Studio 17 2022" -A x64 -S FanFolderCpp

# Run the built exe
& "C:\projekter\win-dir-fan\FanFolderCpp\build\Release\FanFolderCpp.exe"

# Kill running instance before replacing the exe
$p = Get-Process | Where-Object { $_.Name -like "*FanFolderCpp*" }
if ($p) { Stop-Process -Id $p.Id }
```

After building, always restart the running process if it is already running.

---

## Build Commands (C# — reference only, do not use unless asked)

```powershell
dotnet build -c Release
dotnet publish -c Release
```

---

## C++ Architecture (`FanFolderCpp/src/`)

| File | Role |
|---|---|
| `main.cpp` | WinMain: GDI+ init, COM init, Config load, message loop |
| `Config.h/.cpp` | Read all settings from `HKCU\SOFTWARE\FanFolder` registry; fall back to `appsettings.json` → Downloads → Desktop |
| `MainWindow.h/.cpp` | Hidden taskbar window; SC_RESTORE → ToggleFan; WH_MOUSE_LL + WH_KEYBOARD_LL hooks on dedicated thread; prewarm; DWM iconic thumbnail |
| `FanWindow.h/.cpp` | Layered popup (WS_EX_LAYERED \| WS_EX_NOACTIVATE); arc layout; GDI+ rendering; Fan/Glide/Spring/None animations; hover scale + shadow; async icon loading; OLE drag-drop; shell context menu |
| `FileService.h/.cpp` | Folder scan with sort/filter; IShellItemImageFactory bitmap; SHIL icon fallback chain |
| `ShellDrag.h/.cpp` | OLE IDataObject + IDropSource for shell drag-and-drop |

### Key flows

**Taskbar click → fan toggle:**
`WM_SYSCOMMAND (SC_RESTORE)` in `MainWindow::WndProc` → `ToggleFan()` → `OpenFan()` / `CloseFan()` with 250ms cooldown. After `DefWindowProc`, post `WM_MAIN_SHOW_MIN` to re-minimize.

**Fan rendering:**
`FanWindow::DrawToLayeredWindow()` → GDI+ render into DIB → premultiply alpha → `UpdateLayeredWindow`. Timer at 16ms drives both entry animation and hover animation.

**Entry animations (selectable via registry `AnimationStyle`):**
- `Fan` — items fly from arc hinge to rest position with stagger (EaseOutQuart)
- `Glide` — all items drift up 32px and fade in (EaseOutCubic, 800ms)
- `Spring` — window fades in (120ms), items scale 0→1 with stagger + overshoot
- `None` — instant

**Icon extraction fallback chain:**
`IShellItemImageFactory::GetImage` → `SHIL_JUMBO` → `SHIL_EXTRALARGE` → `SHIL_LARGE`

---

## Registry Configuration (`HKCU\SOFTWARE\FanFolder`)

| Value | Type | Default | Description |
|-------|------|---------|-------------|
| `FolderPath` | REG_SZ | Downloads | Folder to display |
| `SortMode` | REG_SZ | `DateModifiedDesc` | `DateModifiedDesc`, `DateModifiedAsc`, `NameAsc`, `NameDesc` |
| `MaxItems` | REG_DWORD | `15` | 1–50 |
| `IncludeDirectories` | REG_DWORD | `1` | `1` = include dirs |
| `FilterRegex` | REG_SZ | *(empty)* | Optional filename filter regex |
| `AnimationStyle` | REG_SZ | `Spring` | `Fan`, `Glide`, `Spring`, `None` |

The C# and C++ versions share the same registry keys — configs are portable between them.

---

## C++ Key Conventions

**Naming:**
- Private fields: `_camelCase`
- Constants: `constexpr` with `camelCase` or `PascalCase`
- Section separators: `// ---------------------------------------------------------------------------`

**Memory / COM:**
- All COM objects released with `->Release()` in `finally`-equivalent (RAII or manual try/finally pattern)
- GDI objects (HBITMAP, HICON, HFONT) deleted with `DeleteObject` / `DestroyIcon` in destructors
- Background icon loading via `std::thread::detach`; results posted back via `PostMessageW`

**Window styles:**
- `FanWindow`: `WS_POPUP | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW`
- `MainWindow`: `WS_OVERLAPPEDWINDOW | WS_MINIMIZE` (needs this for SC_RESTORE from taskbar)

**Arc geometry constants** (in `FanWindow.cpp`): `ArcSpreadPerItem`, `MaxArcSpreadDeg`, `FormMargin`, `LabelGap`

**Animation constants** (in `FanWindow.cpp`): `HoverScaleMax = 1.4f`, `AnimSpeed_In = 0.30f`, `AnimSpeed_Out = 0.38f`, `EntryFadeDurationMs = 120f`, `ItemStageDurationMs = 28f`, `ItemAnimDurationMs = 420f`

---

## C# Reference Version (`FanFolderApp/`) — Feature Specification

The C# version defines the **complete feature set** for the C++ version. When a feature is missing or behaves differently in C++, the C# implementation is the authoritative specification. Key files:

| File | What it specifies |
|---|---|
| `Program.cs` | Registry keys, all config values and defaults |
| `FanForm.cs` | Arc layout math, animation easing functions, label rendering, hover shadow, arrow icon drawing |
| `MainHiddenForm.cs` | Taskbar button behavior, prewarm lifecycle, hook setup |
| `FileService.cs` | Icon extraction logic, archive detection, image thumbnail handling |
