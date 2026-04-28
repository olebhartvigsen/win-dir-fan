# FanFolder v1.2.0

Date: 2026-04-28

Changelog since v1.1.7:
- Improved rendering reliability: the fan popup now renders correctly in sandboxed, headless, and remote-desktop environments where it could previously appear blank or corrupted on first launch.
- Added GDI+ warm-up on startup to eliminate cold-start rendering failures in restricted environments such as Windows Sandbox and validation VMs.
- The rendering pipeline now gracefully recovers from transient graphics failures instead of showing a blank frame — it automatically retries on the next animation tick.
- Fixed a potential blank frame during desktop switches, session locks, or RDP reconnects by retrying the window composition when the screen device context is temporarily invalid.
