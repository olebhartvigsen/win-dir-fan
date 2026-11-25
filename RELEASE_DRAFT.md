# FanFolder v1.5.0

Date: 2026-11-25

Changelog since v1.4.3:
- Filter as you type: open the fan and type. Matching runs across every file in the folder, and in its subfolders when that option is on, from a name index built at startup, so a keystroke no longer walks the disk.
- The search field is a real input: fixed width centred on the screen, the menu font, a blinking caret right after the query, and a match counter at the right edge ("15 of 365", or "365+" when the list is capped).
- New option "Include files in subfolders" lists files below the selected folder and keeps the list current as the folder changes; depth 8, capped at 20,000 files. "Include folders" is greyed out while it is on, because the two options describe different levels of the tree.
- The index is patched from folder-change notifications one directory at a time, saved to %LOCALAPPDATA% on exit and reused on the next start when the folder timestamps still match.
- A spinning ring on the taskbar button and the tray icon shows when menu content is being scanned and when a search is waiting for it.
- New first-launch info panel with links, and a "Show info on launch" toggle in the menu.
- Updates are reported anonymously (app_updated) on the first launch after a version change.
- Security hardening: file drops and parsing no longer trust pointers or sizes passed in window messages, the install ID is unpredictable, the telemetry opt-out fails closed, and protocol URLs are validated before they are opened.
- Fan labels show the file name only, without the subfolder path.
- Rescans are skipped when the folder timestamp is unchanged.
- The settings menu shows the app version at the bottom.
- Known limits: typing while filtering uses a raw keyboard hook, so IME input (Japanese, Chinese, Korean) does not work there; queries are capped at 64 characters; junctions and hidden or system folders are skipped.
