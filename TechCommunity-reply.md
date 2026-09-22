# TechCommunity-svar + diagram

## Diagram (macOS → FanFolder mapping)

Kilde: `/tmp/fanfolder-macos-map.mmd` (ikke committed)
SVG: `/tmp/fanfolder-macos-map.svg` (18 kb, renderet via kroki)

Hvis du vil have diagrammet på hjemmesiden, læg SVG'en i dist-repoet
(`olebhartvigsen/FanFolder`) og referér til den i index.html under
macOS-sektionen. Sig til, så gør jeg det.

---

## TechCommunity-svar (klar til tråden)

Tråd: "Does Windows 11 have a desktop stack feature similar to the one on
Apple Macs?" (maj 2026, techcommunity.microsoft.com/discussions/windows11/
does-windows-11-have-a-desktop-stack-feature-similar-to-the-one-on-apple-macs/4519783)

> Short answer: no, Windows has no native equivalent of macOS Stacks, and
> there's no sign Microsoft is about to add one.
>
> For the desktop-file-organization part, Stardock Fences is the closest
> commercial option, as already mentioned.
>
> For the part most people actually use Stacks for, getting at your recent
> files and Downloads folder without opening Finder/Explorer, I made a small
> free tool that does exactly that: FanFolder. Click the taskbar/tray icon
> and your recent files fan out from the taskbar, macOS Stacks style. You
> can also point it at Downloads or any other folder. It's native C++, about
> 1.4 MB, no .NET runtime, and it reads Windows Jump Lists, so it shows the
> same recent-document data Windows itself tracks.
>
> Homepage: https://olebhartvigsen.github.io/FanFolder/ (winget
> install OleBhartvigsen.FanFolder). Full disclosure: I'm the developer.

Note: this reply is intentionally honest about what it doesn't do (desktop
organization, like Fences) so it doesn't look like a drive-by ad. Only post
if you support disclose-self-promotion in the thread, read the thread's
rules first.