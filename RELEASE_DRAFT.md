# FanFolder v1.3.0

Date: 2026-08-03

Changelog since v1.2.2:
- Added "Open FanFolder homepage" to the tray icon's right-click menu, opening the project homepage in the default browser.
- Added anonymous first-run install reporting through Aptabase (EU-hosted, privacy-friendly). A single `app_installed` event is sent once per installation with no personal data, file paths, or usage details. Opt out via `TelemetryEnabled=0` (REG_DWORD) in `HKCU\SOFTWARE\FanFolder`; see PRIVACY.md for full details.
- Added privacy documentation (PRIVACY.md) describing the telemetry data flow, retention, and opt-out instructions.
