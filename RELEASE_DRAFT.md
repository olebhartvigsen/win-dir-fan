# FanFolder v1.1.0

Date: 2026-04-16

Highlights:
- Added multi-architecture release pipeline with x64 + ARM64 builds.
- Added combined Burn bootstrapper installer for easier distribution.
- Refined animations (Glide/Fan) with per-item stagger and smoother easing.
- Updated branding and visuals, including new app/taskbar icon work.
- Included performance and correctness fixes and UI/behavior polish.

Changelog since v1.0.2:
- Disable WiX auto-glob, explicitly include only `FanFolder.wxs`.
- Exclude `bundle/**` from MSI project to prevent recursive WiX glob.
- Move bundle project to `installer/bundle/` to avoid WiX auto-include.
- Exclude `FanFolderBundle.wxs` from MSI project.
- Add ARM64 build + combined Burn bundle installer; add `workflow_dispatch`.
- Refine app icon and fan layout.
- Rename to FanFolder, dynamic taskbar icon, redesigned app icons.
- Improve Glide and Fan animations: per-item stagger, smoother easing.
- Performance and correctness optimizations + licensing updates.
- UX fix: hide irrelevant sort options per folder mode.
- Fix ghost duplicate in "Seneste filer".

Release process used:

1) Create and push tag:

   git tag -a v1.1.0 -m "Release v1.1.0"
   git push origin v1.1.0

2) Publish GitHub release:

   gh release create v1.1.0 --title "v1.1.0" --notes-file RELEASE_DRAFT.md --repo olebhartvigsen/win-dir-fan

CI notes:
- Publishing this release triggers `.github/workflows/release.yml`.
- Workflow builds Windows artifacts and uploads release assets automatically.
