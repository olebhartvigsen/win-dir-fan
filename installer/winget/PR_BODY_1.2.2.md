## FanFolder 1.2.2

New package version for OleBhartvigsen.FanFolder. Manifests are the ones published alongside the v1.2.2 release and validated locally with `winget validate`.

### Note for the validator

FanFolder lives in the taskbar. To open the fan, single-click its taskbar icon (there's no main window that comes up on its own).

Heads up on one thing you'll likely hit in a clean sandbox: the default source is the Windows recent-documents list, and on a fresh VM that list is empty. When it's empty the fan now shows a small "This folder is empty" item. Earlier versions drew an empty window with a spinner instead, which looked like a hang. That was fixed in 1.2.2, so if you see the "empty" message, that's the app working correctly, not stalling.

Everything installs per-user (`Scope: user`), so no elevation is needed or requested.

### Installer details
- Per-user MSI, x64 and ARM64
- Silent install verified: `/quiet /norestart`
- SHA256 and ProductCode taken from the shipped v1.2.2 MSIs
