# FanFolder – Copilot Instructions

## ⚠️ Git Policy — NEVER commit, push, or merge without explicit user instruction

**Never** run `git commit`, `git push`, `git merge`, or any equivalent (e.g. `git rebase`, `git tag`) unless the user explicitly asks (e.g. "commit", "push", "merge to master"). The user always initiates these actions.

## ⚠️ Winget Policy — NEVER submit a winget release without explicit user instruction

**Never** submit, create, or trigger a winget package update/PR unless the user explicitly asks (e.g. "submit to winget", "do a winget push"). The user always initiates winget submissions.

---

## Project Overview

**FanFolder** is a Win32/C++ desktop app that replicates the macOS Dock "Fan" folder on the Windows taskbar. Clicking a taskbar icon reveals an animated, arc-shaped popup showing the most recently modified items in a configured folder. Items can be opened, right-clicked (full shell context menu), or dragged to other applications.

- **Implementation:** `FanFolder/` — C++20, Win32, GDI+, CMake, MSVC, ~161 KB exe
- No unit tests; no linting/formatting tooling configured

### Repository structure

| Repo | Purpose |
|---|---|
| [`olebhartvigsen/win-dir-fan`](https://github.com/olebhartvigsen/win-dir-fan) | Source code, build pipeline, installer project, winget manifest templates |
| [`olebhartvigsen/FanFolder`](https://github.com/olebhartvigsen/FanFolder) | **Distribution repo** — all releases are published here: `.exe`, `.msi`, bundle installer, and auto-generated winget manifests |

**Release flow:** Every build (tagged or manual) triggers the GitHub Actions pipeline in `win-dir-fan`, which builds x64 + ARM64, bundles a combined installer, auto-generates winget manifests with correct SHA256 hashes and ProductCodes, and publishes everything as a release in `olebhartvigsen/FanFolder`.

**Winget submission:** When ready to submit to winget, download the three manifest files (`OleBhartvigsen.FanFolder.yaml`, `OleBhartvigsen.FanFolder.installer.yaml`, `OleBhartvigsen.FanFolder.locale.en-US.yaml`) from the corresponding release in `olebhartvigsen/FanFolder` — they are ready to submit as-is.

---

## Build Commands

```powershell
# Build (requires Visual Studio 2022 with C++ workload)
cmake --build FanFolder\build --config Release

# First-time cmake configure (if build\ doesn't exist)
cmake -B FanFolder\build -G "Visual Studio 17 2022" -A x64 -S FanFolder

# Run the built exe
& "C:\projekter\win-dir-fan\FanFolder\build\Release\FanFolder.exe"

# Kill running instance before replacing the exe
$p = Get-Process | Where-Object { $_.Name -like "*FanFolder*" }
if ($p) { Stop-Process -Id $p.Id }
```

After building, always restart the running process if it is already running.

---

## Architecture (`FanFolder/src/`)

| File | Role |
|---|---|
| `main.cpp` | WinMain: GDI+ init, OLE init, Config load, message loop |
| `Config.h/.cpp` | Read/write all settings from `HKCU\SOFTWARE\FanFolder` registry |
| `MainWindow.h/.cpp` | Hidden taskbar window; SC_RESTORE → ToggleFan; WH_MOUSE_LL + WH_KEYBOARD_LL hooks on dedicated thread; prewarm; DWM iconic thumbnail; tray icon settings menu |
| `FanWindow.h/.cpp` | Layered popup (WS_EX_LAYERED \| WS_EX_NOACTIVATE); arc layout; GDI+ rendering; Fan/Glide/Spring/Fade/None animations; hover scale + shadow; async icon loading; OLE drag-drop source + target; shell context menu |
| `FileService.h/.cpp` | Folder scan with sort/filter; IShellItemImageFactory bitmap; SHIL icon fallback chain |
| `ShellDrag.h/.cpp` | OLE IDataObject + IDropSource for shell drag-and-drop with ghost image |

### Key flows

**Taskbar click → fan toggle:**
`WM_SYSCOMMAND (SC_RESTORE)` in `MainWindow::WndProc` → `ToggleFan()` → `OpenFan()` / `CloseFan()` with 250ms cooldown. After `DefWindowProc`, post `WM_MAIN_SHOW_MIN` to re-minimize.

**Fan rendering:**
`FanWindow::DrawToLayeredWindow()` → GDI+ render into DIB → premultiply alpha → `UpdateLayeredWindow`. Timer at 16ms drives both entry animation and hover animation.

**Entry animations (selectable via registry `AnimationStyle`):**
- `Fan` — items fly from arc hinge to rest position with stagger (EaseOutQuart)
- `Glide` — all items drift up 32px and fade in (EaseOutCubic, 800ms)
- `Spring` — window fades in (120ms), items scale 0→1 with stagger + overshoot
- `Fade` — instant layout, short fade-in only
- `None` — instant

**Icon extraction fallback chain:**
`IShellItemImageFactory::GetImage` → `SHIL_JUMBO` → `SHIL_EXTRALARGE` → `SHIL_LARGE`

---

## Registry Configuration (`HKCU\SOFTWARE\FanFolder`)

| Value | Type | Default | Description |
|-------|------|---------|-------------|
| `FolderPath` | REG_SZ | Downloads | Folder to display |
| `SortMode` | REG_SZ | `DateModifiedDesc` | `DateModifiedDesc`, `DateModifiedAsc`, `DateCreatedDesc`, `DateCreatedAsc`, `NameAsc`, `NameDesc` |
| `MaxItems` | REG_DWORD | `15` | 5–25 (steps of 5) |
| `IncludeDirectories` | REG_DWORD | `1` | `1` = include dirs |
| `ShowExtensions` | REG_DWORD | `0` | `1` = show file extensions in labels |
| `FilterRegex` | REG_SZ | *(empty)* | Optional filename filter regex |
| `AnimationStyle` | REG_SZ | `Spring` | `Fan`, `Glide`, `Spring`, `Fade`, `None` |

---

## Key Conventions

**Naming:**
- Private fields: `_camelCase`
- Constants: `constexpr` with `camelCase` or `PascalCase`
- Section separators: `// ───────────────────────────────────────────────────────────────────────────`

**Memory / COM:**
- All COM objects released with `->Release()` in RAII or manual cleanup
- GDI objects (HBITMAP, HICON, HFONT) deleted with `DeleteObject` / `DestroyIcon` in destructors
- Background icon loading via `std::thread::detach`; results posted back via `PostMessageW`

**Window styles:**
- `FanWindow`: `WS_POPUP | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW`
- `MainWindow`: `WS_OVERLAPPEDWINDOW | WS_MINIMIZE` (needs this for SC_RESTORE from taskbar)

**Arc geometry constants** (in `FanWindow.cpp`): `ArcSpreadPerItem`, `MaxArcSpreadDeg`, `FormMargin`, `LabelGap`

**Animation constants** (in `FanWindow.cpp`): `HoverScaleMax = 1.4f`, `AnimSpeed_In = 0.30f`, `AnimSpeed_Out = 0.38f`, `EntryFadeDurationMs = 120f`, `ItemStageDurationMs = 28f`, `ItemAnimDurationMs = 420f`

