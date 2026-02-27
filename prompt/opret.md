## Prompt: Build a Windows Taskbar “Fan Folder” App (C#, .NET 8, WinForms)

### Objective
Create a Windows desktop application in C# (.NET 8, WinForms) that replicates the macOS Dock “Fan” folder behavior for the Windows taskbar.

The app must:
- Live in the Windows taskbar (not system tray)
- Show only the app icon in taskbar
- Toggle a fan-style popup of folder contents
- Display only the 15 most recently modified items
- Render the newest item at the bottom (nearest the taskbar)

No Electron. No tray icon.

---

### Core Architecture

#### 1) Hidden Main Window
Create a main form that is never visible, but exists to keep the icon in the taskbar:
- `Opacity = 0`
- `ShowInTaskbar = true`
- `WindowState = Minimized`
- `Text = ""` (no taskbar label)
- Use a high-quality `.ico` file

#### 2) Activation Logic
Use either overridden `WndProc` or `Activated` event:
- Clicking taskbar icon activates the hidden form
- Activation toggles the fan window
- If fan is open, close it
- If fan is closed, open it

---

### Fan Window (UI)

#### Form Style
- Borderless
- Top-most (`TopMost = true`)
- `FormBorderStyle = None`
- `ShowInTaskbar = false`

#### Layout and Constraints
- Display max 15 items
- If folder has more, show only 15 newest by `LastWriteTime`
- Sort descending by `LastWriteTime`
- Newest item must appear at bottom (closest to taskbar)
- Vertical fan expands upward (for bottom taskbar)
- Each item:
  - Height: 40px
  - Icon: 32x32 on the right
  - Text: right-aligned on the left
  - Font: "Segoe UI Variable Text" or "Segoe UI", 9pt

#### Positioning
- Detect taskbar edge: Bottom, Top, Left, Right
- Fan appears flush against taskbar edge
- Fan is horizontally centered over mouse click coordinates (or edge-aligned equivalent for vertical taskbars)

---

### Technical Requirements

#### 1) DPI Awareness
Application must be Per-Monitor DPI aware.

Include in app manifest:
```xml
<dpiAwareness>PerMonitorV2</dpiAwareness>
```

#### 2) High-Quality Native Icons
Use native shell icon extraction via P/Invoke:
- `SHGetImageList` with `SHIL_LARGE`

Icons must be crisp and native-looking.

#### 3) Configuration
Use `Microsoft.Extensions.Configuration.Json` to read `FanFolderPath` from `appsettings.json`.

Example:
```json
{
  "FanFolderPath": "C:\\MyFolder"
}
```

#### 4) Interaction Rules
- File or app: open using
```csharp
Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
```
- Folder: open with `explorer.exe`
- Dismiss fan when user clicks outside (`Deactivate` event)

---

### Required Project Structure
```text
/FanFolderApp
 ├── Program.cs
 ├── MainHiddenForm.cs
 ├── FanForm.cs
 ├── NativeMethods.cs
 ├── FileService.cs
 ├── appsettings.json
 ├── app.manifest
```

File responsibilities:
- `Program.cs`: Entry point, DPI setup, hidden main form context
- `MainHiddenForm.cs`: Taskbar icon host + activation/toggle behavior
- `FanForm.cs`: Fan UI and rendering logic (limit 15)
- `NativeMethods.cs`: P/Invoke declarations for Shell32, User32, Dwmapi
- `FileService.cs`: Fetch top 15 items, sorting, icon extraction
- `appsettings.json`: `FanFolderPath` configuration

---

### Error Handling and UX
- Handle invalid/missing folder path gracefully (no crash)
- Show a user-friendly fallback state for "Path Not Found"
- Include subtle readability enhancement behind items:
  - Background panel and/or
  - Drop shadow

---

### Output Requirements
Generate the complete, runnable C# code for all required files.

Ensure output includes:
- All WinForms code
- All required P/Invoke interop code
- Configuration loading logic
- Taskbar positioning logic
- Graceful path error handling

---

### Non-Negotiable Constraints
- Maximum 15 items shown
- Most recent item at bottom near taskbar
- No system tray or `NotifyIcon`
- Native-quality shell icons
- Per-monitor DPI awareness
- Clean production-quality code (no placeholders)



