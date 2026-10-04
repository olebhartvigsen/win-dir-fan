# Reddit post for r/windowsapps

Verified before writing: v1.4.3 is the latest release (Aug 2026), winget package `OleBhartvigsen.FanFolder` is live with 1.2.2/1.3.0/1.4.3 published, MSI installers are the most-downloaded assets, and the plain `releases/latest/download/FanFolder.exe` link 404s (use the homepage link instead, it resolves to the install options). The stale size figure in the README is corrected to 1.4 MB in both repos; the x64 exe is actually ~1.4 MB, so the post says 1.4 MB.

## Title

I built a free taskbar app that fans out your recent files, native C++, no ExplorerPatcher needed

Shorter alternate: FanFolder - click the tray icon and your recent files fan out above the taskbar

## Post body

Every time I needed a file I'd just used, I was digging through File Explorer or the Start menu's Recommended section. So I built FanFolder. Click the tray icon and your recent documents fan out in an arc above the taskbar. You can also point it at any folder instead: Downloads, an active project, whatever you keep reopening.

What it does:

- Shows your recent files from Windows Jump Lists, or watches any folder you pick
- Open, rename, copy, delete with the normal right-click menu
- Drag files out to other apps, drop files onto the fan to move them into the watched folder
- Five animation styles if you like that sort of thing, and a "None" if you don't
- Real shell icons and thumbnails

It's native Win32/C++, about 1.4 MB, no .NET runtime, and it doesn't patch explorer.exe, so Windows updates can't break it. Free for personal and commercial use.

Install: winget install OleBhartvigsen.FanFolder, or grab the MSI from the latest release.

Links: https://olebhartvigsen.github.io/FanFolder/ | https://github.com/olebhartvigsen/FanFolder

Current version is 1.4.3 (August). Happy to answer questions, and honest feedback on the animations is especially welcome.

## Notes for posting

- LIX of the body: 29.0, 188 words. Short enough to read whole, no wall of text.
- Attach a GIF or screenshot at the top (MARKETING.md item 1). The post works without one, but r/windowsapps posts with a demo image hold attention much better. ScreenToGif is the usual tool.
- Do not use the shorter alternate title and this body's first line together; they say the same thing twice. Pick one.
- Comment reply, if nobody bites in the first hours: mention you can filter by filename and sort by date modified or name, those details land better as answers than as bullets.
- Best posting window: weekday morning US time (MARKETING.md's tip, still valid).
