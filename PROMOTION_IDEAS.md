# FanFolder: three promotion ideas (deep dive, Sep 2026)

This is a second, more aggressive round of promotion ideas, focused on the question that matters most:
people searching for a Windows replacement of macOS functionality should find FanFolder.

## The core insight: FanFolder already is a macOS feature

The macOS feature people miss is not the dock itself. It is the **Stacks** (click a dock stack, and the
files fan out in an arc, grid, or list) and **Recents** (Finder sidebar and right-click on the dock
icon). FanFolder maps to both, exactly:

| macOS feature | What it does | FanFolder equivalent |
|---|---|---|
| Dock Stacks, fan view | Click the Downloads/Files stack, documents fan out in an arc | The arc popup of files from the tray icon, animation style "Fan" |
| Dock icon right-click → Recents | List of recently opened documents for that app | `::RecentDocs::` reads Windows Jump Lists, the same underlying data |
| Finder → Recents sidebar | Recently opened files across all apps | `::RecentFiles::` and `::GraphRecent::` modes |
| Downloads stack on the dock | One click to reach the folder you live in | Point FanFolder at your Downloads folder |

No Windows dock clone (MyDockFinder, ExtraDock, RocketDock, Start11) does the **recent files fan** from
Jump Lists. They clone the look of the dock, not the Stacks behavior. That is the gap FanFolder owns,
and nobody is talking about it in those words yet.

## Evidence this demand is real and unserved

- **Bing autocomplete, checked Sep 2026.** "mac dock on windows", "mac dock on windows 11", "mac dock
  on windows free", "mac os dock for windows 10", "mac dock for pc" are all live repeated queries.
  Meanwhile "macos stacks for windows" returns **no useful completions at all**. The exact feature
  name is an open field nobody is targeting.
- **Microsoft's own community forum, May 14 2026:** "Does Windows 11 have a desktop stack feature
  similar to the one on Apple Macs?" (techcommunity.microsoft.com/discussions/windows11/
  does-windows-11-have-a-desktop-stack-feature-similar-to-the-one-on-apple-macs/4519783). The answers
  are "no" and "use Stardock Fences". The demand is sitting there unanswered.
- **Mainstream press covers this desire.** XDA ran "Windows 11's taskbar might get my favorite macOS
  Dock animation soon" in 2026. YouTube has a whole genre: "Make Windows 11 look EXACTLY like macOS",
  "I Made Windows 11 Feel Like macOS With Just 2 FREE Apps".
- **Forced switchers exist in volume.** r/MacOS threads from 2026 include "everyone has to switch to
  Windows and I'm seriously dreading it" and "Switched from MacBook to a Windows Laptop: my
  impressions". These people search for the features they lost.
- **The homepage today has zero macOS framing.** Title is "FanFolder: Quick Access to Recent Files on
  Windows". The page never mentions Stacks, dock, or macOS. A Mac user searching for what they lost
  will not even know FanFolder is the answer.

---

## Idea 1: Own the "Mac on Windows" search corridor

**Goal:** when a switcher searches "mac dock on windows" or "macos stacks for windows", FanFolder is
the third result, behind the usual dock-clone review pages.

### 1a. A dedicated landing section on the homepage

Add a section (own page or large block): "**The macOS Stacks feature, on the Windows taskbar**".

- Headline uses the search words: "macOS Stacks and recent files for Windows".
- Show the mapping table above (Stacks → FanFolder, Recents → FanFolder, Downloads stack →
  FanFolder).
- One GIF of the fan opening, nothing else above the fold.
- Honest positioning, this is what makes it credible: "FanFolder is not a dock clone. It is the piece
  of the dock that actually saves time: your recent files and Downloads, one click away, in a fan."
- Change the page title to something like "FanFolder: macOS Stacks-style recent files fan for the
  Windows taskbar". The current title optimizes for nothing.

### 1b. Metadata: winget, Microsoft Store, AlternativeTo

- winget: the current manifests have **no Tags field** (checked the winget-pkgs manifests for 1.4.3).
  Add Tags via a manifest PR: `taskbar launcher recent-files dock stacks macos quick-launch
  jump-lists`. Winget search is keyword driven, so this is free discovery.
- Microsoft Store: add the same keywords to the search keywords field, and add "macOS" and "Stacks"
  somewhere honest in the description ("brings the macOS Dock Stacks way of reaching files to
  Windows").
- AlternativeTo: add FanFolder as an alternative to **MyDockFinder, ExtraDock, RocketDock, Start11
  and ExplorerPatcher** (the "tools people install to get macOS dock feel or old taskbar back"
  category, which currently has no recent-files fan entry).

### 1c. Answer the demand where it sits

- Reply in the Microsoft TechCommunity Stacks thread above. It is open, recent, and currently
  answered only with "no" and "use Stardock Fences". Give the honest answer (Windows has no native
  Stacks) and offer FanFolder for the file part. Microsoft forums allow self-disclosure answers when
  they solve the question.
- Reply in r/MacOS and r/laptops "just switched" threads when they mention the dock or files. The
  pattern from MARKETING_THREADS.md applies: lead with the fix, disclose, don't link-dump.
- Add a downloadable "for switchers" one-pager GIF titled "Your Downloads stack, on Windows", for
  later use in creator outreach.

### 1d. The press/creator angle

The news hook writes itself: XDA just wrote that the taskbar **might** get the macOS dock animation.
The follow-up story: "it already has the useful part, on the taskbar today, in 1.4 MB, free". Pitch
XDA, Windows Central, Neowin and the YouTube channels that made "make Windows 11 look like macOS"
videos. Offer them a short segment (GIF + 3 facts), not a press release.

**Effort:** one afternoon for the page + metadata, one hour a week for replies. **Impact:** the
queries have weak competition and the demand is written down in autocomplete, forums, and news
headlines. This is the single best new-user channel available.

---

## Idea 2: Turn verified pain questions into owned pages (the search-intent engine)

**Goal:** rank for the questions people actually type, not for the product name. Every thread we
answered in MARKETING_THREADS.md is a search query someone types every day.

### 2a. A small "guides" section on the homepage

One page per verified pain point, written as a neutral guide with FanFolder as the pragmatic fix:

- "How to get the Quick Launch bar or taskbar toolbars back in Windows 11" (covers ExplorerPatcher
  and Start11 honestly, then the no-patching option)
- "Windows 11 recent files not showing: fixes and workarounds"
- "Jump lists missing or broken in Windows 11"
- "Pin files to the taskbar in Windows 11"
- "macOS Stacks on Windows: the real options in 2026" (ties into Idea 1)

The pages already exist in raw form: the homepage FAQ and the thread answers in
MARKETING_THREADS.md. Assembly work, not writing work. Add FAQ/HowTo schema markup so the pages can
win rich results. A GitHub Pages site can do all of this with zero cost.

### 2b. The monthly loop that keeps it compounding

The tooling already exists (reddit.py + web_search). Once a month:

1. Pull new thread titles for the key phrases (quick launch, toolbars, recent files, jump list,
   mac dock on windows).
2. Update or add one guide page for any repeating question.
3. Answer the new threads with a link to the matching page.

Each answered thread is a backlink and a relevance signal. After six months the guides outrank the
forum threads they started from, because forum threads are locked in time and the guides keep
getting updated. This is a loop, not a campaign.

**Effort:** 2 hours a month once the templates exist. **Impact:** compounds; each page can keep
converting search traffic for years.

---

## Idea 3: Assemble where the "macOS on Windows" audience already gathers

**Goal:** borrow existing demand instead of building it from zero. Three concrete moves, all verified
to exist:

### 3a. Get into the curated dock-alternative lists

ExtraDock runs "8 Best macOS Dock Alternatives in 2026" (extradock.app/alternatives/
best-macos-dock-alternatives). Someone curates these pages. Ask to be added as "the Stacks/recent
files companion" or as an alternative, for both ExtraDock and MyDockFinder ("the free way to get the
Stacks part of the dock"). The same for every "macOS dock for Windows" roundup that accepts
suggestions. One polite message per list, with the GIF.

### 3b. Ride the "I have to switch to Windows" threads

These are not only in r/windows* subs. Verified locations from Sep 2026: r/MacOS ("dreading the
switch"), r/laptops ("Switched from MacBook to a Windows Laptop: my impressions"), r/dotnet,
r/MacOS again ("Former Windows users, how do you deal with..."). When the conversation touches the
dock, recents, or file access, the answer is the Stacks mapping. These posts have hundreds of
engaged comments and the audience is exactly the people who will search later.

### 3c. The Feedback Hub ally move

Microsoft's Feedback Hub has long-running requests for toolbars/stacks/recents restore. Link the
relevant feedback item on the homepage under the FAQ: "Want these features natively in Windows? Vote
here. Want them today? Download FanFolder." This positions FanFolder as a stopgap to a feature
Microsoft itself acknowledges people want, and every search for "feedback hub stacks windows" finds
the page. Find the best matching feedback item (search "stacks" and "taskbar toolbars" in the
Feedback Hub) and link it.

**Effort:** low, mostly copy-paste outreach. **Impact:** each list page and each big switcher thread
is a one-time spike, but they are the spikes that make the SEO in Ideas 1 and 2 pay off faster.

---

## What I would do first, in order

1. Homepage: the macOS section + title change (Idea 1a). This is the highest value per hour spent.
2. Winget Tags + Store keywords + AlternativeTo entries (Idea 1b). One evening.
3. The TechCommunity thread + 2 fresh switcher threads (Ideas 1c + 3b). One hour.
4. The first three guide pages (Idea 2a) and one month of the loop.
5. The curated-list and XDA/creator pitches (3a + 1d) once the homepage section exists, so every
   pitch has a page to point at.

Everything here is free except time. The 161 KB era is over, the app is 1.4 MB, signed, on winget
and in the Store. The missing piece was never the product. It was that nobody on the internet has
yet said, in the words switchers use, "this is the macOS Stacks feature for Windows".