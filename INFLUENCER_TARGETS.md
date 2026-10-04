# Influencer, creator and press targets

Verified 3 October 2026. Read the "reachability" column before anything else: three of the outlets you
would naturally pitch are **blocked from this container**, so their contact details are reported from search
results and public pages rather than fetched. Confirm each address from a normal browser before sending.

## The finding that changes the priority order

Your marketing plan puts press and creators at action 9, below the thread work. That ranking is right, but
it misses a bigger, adjacent audience: **the Windows taskbar-customisation crowd already sits inside
Windhawk**, and it is large enough to dwarf the press contacts.

| Windhawk mod | Users |
|---|---|
| Windows 11 Taskbar Styler | 971,751 |
| Taskbar Dock Animation (macOS-style icon scaling) | 428,460 |
| Taskbar Positions & Smooth Physics | 6,200+ |

Windhawk calls itself "the customization marketplace for Windows". Its users have already installed a
shell-tweaking tool and told a stranger they want macOS behaviour on their taskbar. FanFolder does not
replace that tool, it complements it, and the pitch is easy: "the part of the dock that saves time, without
patching the shell". There is a public Discord (Windhawk server, id `923944342991818753`), GitHub
Discussions on `ramensoftware/windhawk-mods`, and r/Windhawk.

Treat this as one warm audience reachable today, ahead of any press pitch.

---

## Tier A: reachable now, highest fit

### A1. r/Windhawk and the Windhawk Discord
**Why:** the highest-intent audience found. Members are actively asking for macOS taskbar behaviour. FanFolder
is adjacent to the mods they already run, and the recurring pain (patched shell breaks on feature updates)
is the exact FanFolder argument.
**Reachability:** public subreddit and a public Discord invite. Post about the app, not a mod: FanFolder is a
standalone tray app, so it cannot be submitted to the mod collection and should not be pitched as one.
**Ask:** opinion on the animation and whether anyone would use it beside Windhawk.
**Honest caveat:** FanFolder patches nothing, which in this community can read as the *weak* option. Lead with
that as the feature, not as a limitation.

### A2. Chris Titus Tech
**Why:** published "Windows 11, But I Made It Look Like the Latest macOS for FREE!" (233K views) and
"Windows Utility 2026: The Biggest Update Yet". The macOS-on-Windows format is his recurring series, and
he rounds up dozens of small utilities per video. Channel verified live (HTTP 200).
**Reachability:** public channel. Business email is listed on his site; a channel comment or the contact form
both work, email is better.
**Pitch:** a segment, not a product. Offer the three facts plus a GIF: 1.4 MB, no .NET, does not patch
explorer.exe, reads Jump Lists. He can test it in ten minutes.
**Realistic ceiling:** 10 to 100 installs if it lands.

### A3. The "make Windows 11 look like macOS" genre, beyond one channel
**Why:** this genre repeats across many channels. "I Made Windows 11 Feel Like macOS" (233K views in one
instance), the Windhawk macOS-hover-animation videos, Komorebi/Yasb tiling-setup videos (Diiv, 2.53K subs,
181K views) and REGITEOFFICIAL's Make-Windows-11-Look-Like-macOS repo. The audience is the macOS-switcher
corridor your homepage already targets.
**Reachability:** pick creators whose *recent upload* is about taskbar or taskbar theming. Avoid anyone
whose macOS videos are wallpapers only, they have no use for this.

---

## Tier B: press, none reachable from this container

Neowin and XDA could not be fetched here; Windows Central could. Details for the first two come from search
results and public pages, so confirm each address from a normal browser before sending.

### B1. Neowin — best press target, has a public submission portal
**Why:** a dedicated, documented, public news submission form, and they publish small Windows utility news.
A queue of human-reviewed submissions means a small tool with a clean angle can genuinely get through.
**Submission page:** `https://www.neowin.net/submitnews.php`
**Guidelines:** `https://www.neowin.net/submitnews.php?action=help`
**Reachability:** both returned HTTP 403 to this container, which is Cloudflare, not a dead link. Open in a
browser. Their guide says submissions sit in a queue for minutes to hours, and that they read the guidelines
first and reject low-effort posts.
**Write as news, not as promotion.** Their rules explicitly want original writing, no copy-paste, quality over
quantity. A "here is my app" post gets declined; a "Windows 11 may finally get a movable taskbar, and here is
the part that already exists" post has a chance.

### B2. XDA — has published the exact story you can attach to
**Why:** two verified, on-point articles:
- "Windows 11's taskbar might get my favorite macOS Dock animation soon" (Mahnoor Faisal)
- "Modern Dock brings a touch of macOS to Windows" (João Carrasqueira)

Both are the macOS-on-Windows beat. The second is the better hook: it criticises Modern Dock for not being
integrated with the taskbar, which is exactly the gap FanFolder fills.
**Reachability:** every xda-developers.com URL returned a connection failure to this container, so the contact
form path is unconfirmed from here. Find it from a browser. Author addresses are on the articles themselves,
which is often the better route for a pitch tied to a specific piece.
**Do not re-pitch the same news they already ran.** Pitch the app as the answer to the question their article
raises.

### B3. Windows Central
**Why:** the source of the movable-taskbar reporting that XDA and Neowin both cite. They cover taskbar
customisation heavily.
**Reachability:** homepage returns HTTP 200 from here, so the site is not blocked for this container. No
public tip or submission route was confirmed though, so find the contact address from a browser rather than
assuming a portal exists.
**Note:** they are an affiliate-heavy site; expect a link request, decide in advance whether that is worth it.

### Not worth your time
The Verge, Ars Technica, TechRadar, Guru3D are all reachable but they do not cover small Windows utilities.
Their reader expects news about platforms and policy, not tray apps. This is the classic "wrong list of
contacts" trap: reachable is not the same as relevant.

---

## Tier C: the indirect list, no cold email

These are aggregators. One listing puts FanFolder in front of people already browsing for this exact tool.

- **AlternativeTo**, positioned as an alternative to ExplorerPatcher, Start11, StartAllBack and RocketDock.
  Those pages rank for the problem searches, and several utilities on them are flagged Discontinued, which is a
  search slot going cold. Draft is ready: `ALTERNATIVETO_LISTING.md`.
- **MajorGeeks and FossHub**, copy ready in `installer/directories/`.
- **r/windowsapps Show-and-tell**, draft ready in `REDDIT_POST_r_windowsapps.md`.

---

## What to send each: the three-fact offer

Every pitch, press or creator, is the same three facts plus one asset. Never a press release.

> Free Windows taskbar utility, 1.4 MB, native Win32/C++, no .NET runtime. It fans your recent files out
> from the taskbar in a macOS Stacks-style arc, reading the same Windows Jump Lists the taskbar already uses.
> It does not patch explorer.exe, so Windows feature updates cannot break it. Win32 + C++ source on request.
> Happy to send a GIF or the installer. I built it.

Adjust the last line for press (drop "on request", they will ask) and for creators (offer the ten-minute
test). Disclose authorship in the first line, every time.

## Rules for this outreach

- Do not use `#opensource` anywhere. FanFolder is proprietary with source withheld, and this audience will
  check.
- Do not claim open source, MIT, or "read the code yourself". A false licence claim in a screenshot is a false
  advertisement.
- Never send two identical pitches. Each outlet gets its own angle or its own three facts, in its own order.
- One follow-up, then stop.
- Expect most of this to be silence. One video or one accepted news post is a win; a 10 percent reply rate is
  an excellent result for cold outreach to creators.