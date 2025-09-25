Windows Directory Fan
======================

Lightweight popup “fan” listing the most recent files/directories in a target folder (Finder stack style) built with Python + PySide6. Optimised for Windows with high‑resolution icon extraction (ShellItemImageFactory / HBITMAP conversion) and clean instant display (no entrance animation).

Current Structure
-----------------

- `app.py` – entrypoint; creates a hidden/minimised main window so a taskbar button exists and toggles the fan near the taskbar cursor location.
- `fan_widget.py` – fan popup widget: lays out items on a curved horizontal arc; hover highlight + filename labels on the left; Windows taskbar thumbnail suppressed (blank preview).
- `requirements.txt` – minimal dependency list.
- `pyinstaller.spec` – sample build spec (onedir) using multi‑size application icon when present.
- `app icon.png` / `app.ico` – application icon source + generated multi‑size ICO.

Removed During Cleanup
----------------------

- Entrance/opening animation code (instantly shows now).
- Background thumbnail worker (synchronous icon/icon extraction sufficient).
- Ad‑hoc debug / exploratory scripts and informal tests.

Features
--------

- Shows up to N recent entries (default 10) from a directory (files + subdirectories).
- Optional top “arrow” item opening the base directory.
- High‑resolution Windows icon extraction with caching (temp folder + in‑memory).
- Curved fan layout using configurable sine/exp/quadratic style (default sine) with adjustable spacing.
- Hover glow + subtle scale for icons and highlight adjustment for labels.
- Automatic dynamic thumbnail size based on screen height & item count.
- Taskbar blank iconic thumbnail to avoid distracting preview.
- Logging to `debug.log` (stdout/stderr can be redirected; see Env section).

Installation (Development)
-------------------------

```pwsh
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

Run
---

```pwsh
python app.py "C:\\Windows\\System32"
```
Omit the argument to default to the user home directory.

Environment Variables
---------------------

- `WIN_DIR_FAN_REDIRECT_STD=0` – disable redirection of stdout/stderr into the log file if you prefer console output.

Building (PyInstaller Example)
------------------------------

1. Ensure `app icon.png` exists (multi‑size ICO will be generated on start).
2. Run (PowerShell):
   
```pwsh
pyinstaller --clean --onedir pyinstaller.spec
```

Notes
-----

- Project intentionally small; no external watcher library required.
- Dependencies trimmed to essentials: PySide6, Pillow, numpy, comtypes (Windows icon path). Optional PDF/icon fallbacks handled defensively.
- Logging is verbose under DEBUG; adjust `logging.basicConfig` in `app.py` if desired.

License
-------

Prototype code – adapt freely (add an explicit LICENSE file if distributing).
