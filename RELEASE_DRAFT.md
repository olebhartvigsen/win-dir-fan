# FanFolder v1.1.6

Date: 2026-04-22

Changelog since v1.1.5:
- Added 13 new UI languages — Russian, Hindi, Turkish, Hebrew, Czech, Finnish, Hungarian, Greek, Vietnamese, Indonesian, Ukrainian, Romanian, and Thai — bringing the total to 29 supported languages.
- Fixed the fan opening against the edge of a secondary monitor when launched via Alt+Tab; it now anchors correctly to the taskbar button on whichever monitor you're on.
- Only one FanFolder instance can run at a time — upgrading no longer leaves two copies fighting over the taskbar icon.
- Installer now closes any running FanFolder before upgrading, so you get the new version cleanly without needing to reboot or sign out.
- Fan now opens noticeably faster on repeat uses: icons are pre-converted in the background so reopening the fan skips hundreds of milliseconds of work on the UI thread.
- Eliminated a stability issue where rapidly opening and closing the fan could slowly leak memory and window handles over long sessions.
- Idle CPU usage is now effectively 0% — removed a diagnostic log that was quietly writing to disk on every mouse click and animation tick in Release builds.
