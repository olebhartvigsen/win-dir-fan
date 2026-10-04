# FossHub contact form: ready to send

Go to https://fosshub.com/contact.html

Fill the form fields like this:

- **name**: Ole Bülow Hartvigsen
- **email**: (the address you want FossHub to reply to)
- **subject**: Listing request: FanFolder, freeware taskbar utility
- **website**: https://olebhartvigsen.github.io/FanFolder/
- **message**: paste everything below

---

## Message body

Hello,

I would like to request a FossHub listing for FanFolder, a freeware Windows taskbar utility I develop on my
own. Details and the clean portable build are below.

**What it does**

FanFolder adds one icon to the Windows taskbar. Click it and your recent files fan out from the icon in an
arc. Click a file to open it, drag it into another application, or right-click for the standard Windows shell
context menu.

It can also point at any folder rather than recents, so pointing it at Downloads gives you the Downloads stack
behaviour, and pointing it at a project folder gives you a one-click view of that folder's most recently
changed files.

**Why I think it fits FossHub**

I am listing the portable build only, as you ask for software that ships with an installer. It is a single
native Win32/C++ binary of about 1.4 MB with no runtime dependencies of any kind: no .NET, no VC++
redistributable, no installer, no bundled third-party offers, no advertising and no telemetry beyond a single
optional anonymous first-run event that can be switched off in the registry. I verified the binary contains no
.NET runtime references.

**Download (portable x64, single file)**

https://github.com/olebhartvigsen/FanFolder/releases/latest/download/FanFolder-x64.exe

An ARM64 build is available at the same location with the name FanFolder-arm64.exe. A signed per-user MSI and
a Microsoft Store package also exist if you ever want to list those instead.

**Project page**

https://olebhartvigsen.github.io/FanFolder/

**Details**

- Version: 1.4.3
- License: freeware, free for personal and commercial use, source code not distributed
- Size: 1,488,896 bytes (x64)
- Runtime dependencies: none
- OS: Windows 10 version 2004 or later, and Windows 11
- Architectures: x64 and ARM64
- Administrator rights: not required, runs from a folder
- Languages: 29
- Also on winget as OleBhartvigsen.FanFolder, and in the Microsoft Store

**What it does not do**

It is not a dock clone or a start menu replacement. It does not modify explorer.exe. Several comparable
tools (ExplorerPatcher, StartAllBack, Start11) restore the old Windows 10 taskbar by patching the shell,
which means a Windows update can break them. FanFolder is an ordinary application and does not have that
weakness, but it also does not restore the old taskbar.

Screenshots and a short demo GIF are available on request, or from the project page.

Thank you for your time.

Ole Bülow Hartvigsen
https://olebhartvigsen.github.io/FanFolder/
https://github.com/olebhartvigsen/FanFolder/issues
