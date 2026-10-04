# MajorGeeks submission email: ready to send

Send to **mgnews@majorgeeks.com** (their contact page gives this as the address for content and file
submissions). Attach the five `screenshot-*.png` files and `demo.gif` from this folder.

Suggested subject line:

```
Submission: FanFolder 1.4.3, freeware taskbar fan for recent files
```

---

## Email body

Hello,

I would like to submit FanFolder for consideration on MajorGeeks. It is a freeware Windows taskbar utility
that I develop and maintain myself.

**FanFolder 1.4.3**

Author: Ole Bülow Hartvigsen
License: Freeware (free for personal and commercial use, source not distributed)
Size: 1.4 MB (portable x64 binary)
Requires: Windows 10 version 2004 or later, or Windows 11; x64 or ARM64
Administrator rights: not required
Date: 3 October 2026
Homepage: https://olebhartvigsen.github.io/FanFolder/
Download: https://github.com/olebhartvigsen/FanFolder/releases/latest/download/FanFolder-x64.exe

**Description**

FanFolder adds one icon to the Windows taskbar. Click it and your recent files fan out from the icon in an
arc. Click a file to open it, drag it straight into another application, or right-click for the standard
Windows shell context menu, so copy, rename and delete work the way they do everywhere else.

Leave it pointed at Recent Documents and it collects what you have been working on across every app. It reads
the recent-document data Windows already records in Jump Lists and in Explorer's Recent folder, so it matches
what Windows itself calls recent. That also means it works when the built-in recent list is switched off or
blocked by group policy on a managed PC.

Point it at Downloads, Documents or a project folder instead and it becomes a one-click view of that folder's
most recently changed files. Sort by modified date, created date or name, set how many items appear, filter
filenames with a regular expression, and choose from five animation styles: Fan, Glide, Spring, Fade or None.
Works with local folders and with cloud-synced ones such as OneDrive, Dropbox and Google Drive.

**Technical notes**

Native Win32 and C++. About 1.4 MB. No .NET runtime and no VC++ redistributable, so there are no runtime
dependencies to install. Installs per-user, so it never requests elevation. Ships in 29 languages. It is also
available through winget as OleBhartvigsen.FanFolder and in the Microsoft Store.

**Why it is not a shell patch**

Most tools in this space (ExplorerPatcher, StartAllBack, Start11) restore the old Windows 10 taskbar by
injecting code into explorer.exe. That works until a Windows update changes something the patch depends on,
which regularly leaves users with a broken taskbar. FanFolder is an ordinary tray application and does not
touch the shell, so feature updates cannot break it. To be clear about the trade-off: it does not restore the
old taskbar, and I do not claim it does. It covers the folder-and-recent-files part of that job.

**Telemetry**

One anonymous event on first run so I can tell whether anyone uses the app. No file names, no paths, no
account data. One registry value disables it. Privacy policy:
https://github.com/olebhartvigsen/FanFolder/blob/main/PRIVACY.md

**Support and feedback**

https://github.com/olebhartvigsen/FanFolder/issues

I work on this alone and read all of it. Happy to answer questions or make changes if you decide to list it.

Thank you for your time.

Ole Bülow Hartvigsen
