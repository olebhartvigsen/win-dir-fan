# FanFolder: submission copy for software directories

All copy verified against live sources on 3 October 2026. Sizes and URLs checked against the v1.4.3
release. Both portals need a human with an account, so this file is everything required to submit.

## Verified facts, use these and nothing else

| Fact | Value | Source |
|---|---|---|
| Product name | FanFolder | |
| Version | 1.4.3 | latest release tag |
| License | Freeware, free for personal and commercial use, source not distributed | LICENSE.md |
| Portable x64 binary | 1,488,896 bytes (about 1.4 MB) | release asset |
| Portable ARM64 binary | 1,436,160 bytes | release asset |
| MSI x64 / ARM64 | 1,323,008 / 1,261,568 bytes | release asset |
| Runtime dependencies | None. Native Win32/C++, no .NET, no VC++ redistributable | verified: no mscoree/clr/api-ms-win-crt strings in the binary |
| Minimum OS | Windows 10 version 2004 (build 19041) or later | AppxManifest MinVersion |
| Architectures | x64 and ARM64 | |
| Elevation | None. Per-user install, no admin rights | Scope: user in the winget manifest |
| Languages | 29 | Localization.cpp |
| Telemetry | One event on first run, no file names or paths, opt-out registry value | PRIVACY.md |
| winget | `OleBhartvigsen.FanFolder` | manifests/o/OleBhartvigsen/FanFolder/ |
| Microsoft Store | Live, product ID 9P3VT115SBM1 | store listing |
| Homepage | https://olebhartvigsen.github.io/FanFolder/ | live, 200 |
| Issue tracker | https://github.com/olebhartvigsen/FanFolder/issues | |

### Download URLs

Portable, single file, no installer (this is the one FossHub wants):

```
https://github.com/olebhartvigsen/FanFolder/releases/latest/download/FanFolder-x64.exe
https://github.com/olebhartvigsen/FanFolder/releases/latest/download/FanFolder-arm64.exe
```

Installer bundle:

```
https://github.com/olebhartvigsen/FanFolder/releases/latest/download/FanFolderSetup.exe
```

Per-arch MSI:

```
https://github.com/olebhartvigsen/FanFolder/releases/latest/download/FanFolderSetup-x64.msi
https://github.com/olebhartvigsen/FanFolder/releases/latest/download/FanFolderSetup-arm64.msi
```

There is no asset named `FanFolder.exe`. Do not use that name in any listing.

## Screenshot captions

Use these in the same order as the files in this folder.

1. `screenshot-1-hero.png` (2540x1520) - The fan menu open from the taskbar, showing recent files in an arc
2. `screenshot-2-actions.png` (2540x1520) - Open a file, drag it into another app, or use the Windows context menu
3. `screenshot-3-any-folder.png` (2540x1520) - Pointed at a folder instead of recent documents
4. `screenshot-4-customise.png` (2540x1520) - Sorting, item count and display options
5. `screenshot-5-get-it.png` (2540x1520) - The install command

Also available: `demo.gif` (960x540, 66 frames, looping) and `icon-512.png` (512x512).

Note: `FanFolder/windows store/screenshot-1.png` is 1048x1700 (portrait, Store format). Do not use it on
these portals, which want landscape.

---

## Short description (under 200 characters)

FanFolder puts your recent files on the Windows taskbar. Click the icon and they fan out in an arc. Native
Win32, 1.4 MB, no runtime, free for personal and commercial use.

(168 characters)

## One-line pitch

Recent files and any folder, one click from the Windows taskbar.

## Medium description (about 800 characters)

FanFolder adds one icon to the Windows taskbar. Click it and your recent files fan out from the icon in an
arc. Click a file to open it, drag it into another application, or right-click for the standard Windows
shell menu.

Leave it pointed at Recent Documents and it collects what you have been working on across every app, reading
the same data Windows records in Jump Lists. Point it at Downloads, Documents or a project folder instead and
it becomes a one-click view of that folder's most recently changed files. Sort by modified date, created date
or name, set how many items appear, and filter filenames with a regular expression.

FanFolder is native Win32 and C++. It is about 1.4 MB, needs no .NET or VC++ runtime, and installs per-user
without administrator rights. It is a normal tray application, not a shell patch, so a Windows feature update
cannot break it the way it can break ExplorerPatcher or StartAllBack.

Free for personal and commercial use. No account, no trial, no upsell.

## Full description (for portals that want detail)

FanFolder adds one icon to the Windows taskbar. Click it and your recent files fan out from the icon in an
arc. Click a file to open it, drag it straight into another application, or right-click for the standard
Windows shell context menu, so copy, rename and delete work the way they do everywhere else.

Leave it pointed at Recent Documents and it collects what you have been working on across every app. It reads
the recent-document data Windows already records in Jump Lists and in Explorer's Recent folder, so it matches
what Windows itself calls recent. That also means it works when the built-in recent list is switched off or
blocked by policy on a managed PC.

Point it at Downloads, Documents or a project folder instead and it becomes a one-click view of that folder's
most recently changed files. Sort by modified date, created date or name, set how many items appear, filter
filenames with a regular expression, and pick from five animation styles: Fan, Glide, Spring, Fade or None.
Works with local folders and with cloud-synced ones such as OneDrive, Dropbox and Google Drive.

Native Win32 and C++. About 1.4 MB. No .NET runtime, no VC++ redistributable, no runtime dependencies of any
kind. Installs per-user, so it never asks for administrator rights. Runs on Windows 10 version 2004 or later
and Windows 11, on both x64 and ARM64. Ships in 29 languages.

FanFolder is a normal tray application rather than a shell patch. Tools like ExplorerPatcher, StartAllBack and
Start11 restore the old Windows 10 taskbar by injecting code into explorer.exe, which means a Windows update
can break them until a fix ships. FanFolder does not touch the shell, so it keeps working across feature
updates. If you want the whole old taskbar back, those tools are the right answer. If you want one folder or
your recent files within a click, this is.

Free for personal and commercial use, with no account, no trial and no upsell. One optional anonymous
first-run event tells the developer whether anyone is using it; it carries no file names or paths, and a
registry value turns it off.

## Feature bullets (portal style, one line each)

- Recent files on the taskbar, one click away
- Recent documents collected from Windows Jump Lists
- Point it at any folder, including OneDrive, Dropbox and Google Drive
- Open files, drag them into other apps, or use the Windows context menu
- Five animation styles: Fan, Glide, Spring, Fade or None
- Sort by modified date, created date or name
- Filter filenames with a regular expression
- Set the item count, folder visibility and extension display
- Native Win32 and C++, about 1.4 MB, no runtime dependencies
- Per-user install, never asks for administrator rights
- Does not patch explorer.exe, so Windows updates cannot break it
- Windows 10 and 11, x64 and ARM64, 29 languages

## Short feature list (5 items, when space is tight)

- Recent files on the taskbar in one click
- Works with any folder, local or cloud-synced
- Native Win32 and C++, about 1.4 MB, no runtime
- Per-user install, no administrator rights needed
- Does not patch the shell, so Windows updates cannot break it

## Categories

MajorGeeks: System > Utilities (or Desktop Enhancements). Check what the closest listed entries use before
sending.

FossHub: System > Utilities.

## Keywords

taskbar, recent files, jump list, quick launch, file launcher, folder launcher, macOS stacks, dock, launcher,
productivity, Windows 11, Windows 10

## System requirements (MajorGeeks asks for these explicitly)

- OS: Windows 10 version 2004 (build 19041) or later, Windows 11
- Processor: x64 or ARM64
- Memory: negligible, under 50 MB while running
- Disk: about 1.5 MB for the portable build
- Runtime: none
- Administrator rights: not required

## Author / developer blurb

FanFolder is built by one developer, Ole Bülow Hartvigsen, and has been in active development since April
2026. It is free for personal and commercial use.

---

## Notes specific to each portal

### FossHub

Their FAQ is explicit: they accept open source **and some freeware**, and for anything with an installer they
want a **clean portable version** to list instead. Submit `FanFolder-x64.exe` as the listed download, not the
MSI. It is a single native binary with no runtime dependencies, which is exactly what they ask for.

Submission goes through the contact form at https://fosshub.com/contact.html with the fields `name`,
`email`, `subject`, `website`, `message`. There is no longer a developer sign-up form; the signup page now
redirects to the contact form, and FossHub states it does not accept file-hosting account applications at
all, only listing requests. Do not describe the submission as a hosting request.

Set the subject to something like "Listing request: FanFolder, freeware taskbar utility".

### MajorGeeks

There is no self-service submission portal. Their contact page gives one address for content and file
submissions: **mgnews@majorgeeks.com**. Email it with the full description, the portable download URL, the
system requirements, and the screenshots attached.

Two expectations worth knowing before you send:

- They review submissions manually and do not accept everything. Their own forum moderator says so
  directly: "not all softwares submitted will be hosted and that's not just software from the smaller devs
  but the larger ones too."
- Their listing pages show Author, Date, Size, License and Requires fields, so fill those in exactly.

Note the site also runs a "requisite 50" threshold mentioned in their support forum. Expect some patience.

Do not send a form letter. Their contact page opens by saying they receive a massive number of inquiries.
Answer their actual criteria: what it does, what it replaces, why it is not a shell patch.
