Windows Directory Fan (PySide6 prototype)

This prototype shows how to implement a Finder-like "stack fan" view on Windows using Python and PySide6.

Structure
- `app.py` - application entrypoint (system tray + window)
- `fan_widget.py` - custom widget that lays out thumbnails in a fan
- `thumbnail_worker.py` - background thread to generate thumbnails
- `requirements.txt` - dependencies
- `pyinstaller.spec` - starter PyInstaller spec (onedir)

How to run (development)

1. Create a venv and install dependencies:

```pwsh
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

2. Run the app (example directory provided):

```pwsh
python app.py "C:\\Windows\\System32"
```

Notes
- This is an initial prototype: fan layout, tray icon, async thumbnail generation.
- Packaging instructions using PyInstaller are in `pyinstaller.spec` comments.
