# FanFolder v1.2.1

Date: 2026-05-04

Changelog since v1.2.0:
- Optimized first fan opening after system wake or reboot: extended GDI+ warm-up at startup and added session-level warm-up on first fan open to ensure the rendering and icon conversion pipelines are fully initialized.
- On first fan open of each session, the app now preemptively converts a small batch of real folder icons to improve perceived startup responsiveness.
- The combined warm-up strategy eliminates multi-second delays on first-open in slow environments while keeping subsequent opens lightning-fast via the existing prewarm cache.
