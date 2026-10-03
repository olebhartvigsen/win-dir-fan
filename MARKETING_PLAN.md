# FanFolder marketing plan

Ranked by expected installs. Written 3 October 2026 against live data, not the README.

## Where the product actually stands

| Measure | Live value | Checked |
|---|---|---|
| GitHub asset downloads, all time | 932 across 17 releases | GitHub releases API |
| Downloads on v1.4.3 (latest) | 255 | GitHub releases API |
| Approx. daily install rate | 5/day over 6 months | 932 / 180 days |
| GitHub stars / forks | 3 / 0 | GitHub API |
| Reddit mentions | 0 | repo notes + HN/Reddit checks |
| Hacker News mentions | 0 | hn.algolia.com |
| Microsoft Store ratings | 0 (`ratingCount: 0`) | store listing HTML |
| winget versions live | 1.2.2, 1.3.0, 1.4.3 | winget-pkgs master |
| Store listing | live, `9P3VT115SBM1`, at 1.4.3 | store listing HTML |

Distribution is not the problem. winget, the Store, GitHub Releases and the homepage are all live and
all working (both MSIs return HTTP 200, the Store page renders, the homepage is indexed). Five installs
a day for a free taskbar utility is a discovery problem, not a product problem.

Four doors produce installs for an app like this: intent search, peer answers, package-manager and
store browsing, and shares. FanFolder is currently closed in all four. That is what the plan opens.

---

## Tier 0: stop the leaks (about 1 hour, do today)

These do not advertise anything. They stop the installs every other action would earn from being thrown
away.

### 1. Fix the wrong winget command in the two docs people copy from

`MARKETING.md` (lines 49 and 112) and `installer/store/PRODUCTHUNT.md` (lines 34 and 56) all say:

```
winget install FanFolder
```

That package does not exist. The live identifier is `OleBhartvigsen.FanFolder`
(`manifests/o/OleBhartvigsen/FanFolder/` in winget-pkgs). Anyone who copy-pastes from those files gets an
error and leaves. `README.md` and the homepage have the right command, which is why this has survived.

Also delete or correct the "GitHub Releases already set up" section in `MARKETING.md`. It tells you to
share `releases/latest/download/FanFolder.exe`, which 404s. The real shared asset is
`FanFolderSetup.exe` (verified 200) or the per-arch MSIs.

**Impact:** protects every install from every paste of these files. Highest ratio of installs saved to
minutes spent of anything in this document.

### 2. Tags: already live on winget, but the CI generator would drop them

**Correction to an earlier check in this document.** I first reported that the live manifests had no
`Tags` field. That was wrong: the check used `head -30`, which cut off before the Tags block. The live
1.4.3 locale manifest on winget already carries 14 tags, including `macos`, `dock`, `stacks`, `recents`,
`quick-launch` and `jump-lists`. Someone already did this work.

The real problem is one level down. `scripts/generate-winget-manifests.ps1` only emitted 8 tags, and CI
regenerates all three manifests on every release from that script. So the next release would have
shipped a manifest with 6 of the 14 tags silently removed, undoing the fix without anyone noticing until
search placement dropped. Fixed: the generator and the committed template now carry all 17 tags.

Two smaller bugs fixed in the same pass:

- `LicenseUrl` and `CopyrightUrl` pointed at `/blob/main/LICENSE`, but the file in the repo is
  `LICENSE.md`, so both were 404s on the package page. Now `.md`.
- Added `recent-files`, `file-launcher` and `windows-11`, which the live manifest lacked. Verified against
  Bing autocomplete: `recent files on taskbar` returns 10 completions, `jump list windows 11` returns 11,
  `quick launch windows 11` returns 7, `macos dock windows 11 free` returns 8.

Nothing to submit right now. These changes ride the next release, and that release's manifest PR picks
them up automatically. If you want the 3 new tags live before then, they have to ship with a version
bump, since winget does not accept edits to an already-published version.

**Impact:** prevents a silent regression on the next release. No standalone install gain.

### 3. Give the Store listing keywords and its first ratings

The listing is live and current but carried none of the macOS/Stacks framing that is working on the
homepage. Zero occurrences of "macOS" or "Stacks" in the listing HTML, and `ratingCount` is 0.

Keywords and copy are now fixed in the repo, ready to import:
`FanFolder/windows store/Partner Center import - English/FanFolderListing.csv` has a rewritten Description
that names the Stacks and Quick Launch gaps explicitly and states that FanFolder does not patch
explorer.exe, a keyword-carrying ShortDescription, and Feature1-to-3 rewritten around the
differentiators.

Search terms sit at exactly the policy maximum of **seven** (Store Policy 10.1.3: "Search terms must not
exceed seven unique terms or phrases"). An earlier pass here added five more on top of the existing
seven, which would have been rejected at certification. The final seven:

| # | Term | Why this one |
|---|---|---|
| 1 | FanFolder | brand |
| 2 | Windows taskbar | the surface it lives on |
| 3 | recent files | 10 Bing completions |
| 4 | quick launch Windows 11 | 7 completions, and the removed feature |
| 5 | macOS Stacks | open keyword field, the differentiator |
| 6 | jump list | 11 completions, and the data source |
| 7 | mac dock for Windows | the biggest adjacent search |

Do not add an eighth without checking the policy first.

Still yours to do:
1. Import that CSV in Partner Center (Apps and games > FanFolder > Store listing > ... > Import) and
   submit. The CSV is the source of truth; the portal is only where it lands.
2. Get the first 5 ratings. A 0-rating listing converts worse than a 5-rating one. Ask the people who
   already replied in your forum threads, and the people who emailed you.

Editing these fields directly in the Partner Center UI is equally valid, and for a one-off change it is
faster than the CSV round-trip. Keep the CSV in the repo as the source of truth either way, so the next
submission starts from the current text instead of re-typing it. See the API note in the pitfalls
section if you want to automate it.

**Impact:** the keyword half ships today. The ratings half is social proof you can only ask for.

### 4. Clean up the releases page — DONE

There were 35 `dev-*` prereleases in `olebhartvigsen/FanFolder`, each carrying the full 10-asset set. The
releases tab read as a test dump and inflated any download count taken over all tags.

Deleted: all 35 dev releases plus their 35 orphaned git tags (deleting a release does not remove its
tag, so both were needed). The 14 stable `v*` releases and their tags are untouched, and the source repo
`win-dir-fan` was never touched. Verified after: `FanFolderSetup.exe`, both MSIs and the x64 MSIX all
still return 200 on `releases/latest/download/`.

One caveat: the dev builds are created by `workflow_dispatch` on `release.yml`, so the next manual
dispatch will add a new `dev-<sha>` prerelease. Either accept that as normal, or change the workflow to
upload dev artifacts as workflow artifacts instead of releases so the public tags page stays clean.

**Impact:** the releases page now shows 14 real versions. Directory submissions and creator pitches
point people here.

---

## Tier 1: the two doors with the most installs behind them

### 5. Ship the guides as four real pages — DONE

This was the biggest structural gap. The homepage had good guides (5,800 characters of honest text under
`#guides`) and a clean meta description, but `sitemap.xml` contained exactly **one** URL. All that content
competed for one page, so the homepage could not rank for `quick launch windows 11`, `recent files taskbar
windows 11` or `jump list not working windows 11` as a dedicated result.

Shipped 3 October 2026 in the `FanFolder` Pages repo, commit `af83ce4`:

- Four pages at `/guides/quick-launch-windows-11/`, `/guides/recent-files-taskbar/`,
  `/guides/jump-lists-windows-11/` and `/guides/macos-stacks-windows/`. Each has a unique title, meta
  description, canonical URL, table of contents, and a four-question FAQ block.
- `FAQPage` JSON-LD on all four, so they can win FAQ rich results. The homepage has `SoftwareApplication`
  but no `FAQPage`, so it never could.
- The homepage stylesheet externalised to `assets/site.css` so the pages inherit the site design instead
  of duplicating it. `index.html` still renders identically.
- Each homepage guide card now links to its full page, and a fourth card was added for the macOS Stacks
  angle, which previously only existed as the `#macos` band.
- `sitemap.xml` now lists all five URLs.

Verified live after Pages rebuilt: all four pages and `assets/site.css` return 200, every relative link on
every page resolves over HTTP, all four FAQ blocks parse as `FAQPage` with 4 questions, the sitemap lists
5 URLs, and the homepage shows no regression (9 details / 9 summaries, hero video, Store badge and winget
command all intact).

One thing not verified: the rendered appearance. No browser was available in the session, so layout and
the FAQ accordion are confirmed only at the HTML level. Worth a glance.

**Impact:** the largest total volume in this plan. Slow to start (weeks), permanent after that.

### 6. Answer the 10 verified threads in `MARKETING_THREADS.md`

They are already researched, with the reply skeletons written. Nothing has been posted. This is the
highest installs-per-hour action in the whole plan, because it catches the exact intent at the exact
moment someone feels it.

Rhythm that works: two threads per weekday for two weeks, then one a week. Vary the wording every time,
disclose you are the developer in the first comment, lead with the actual answer.

Rough shape of what each one is worth: an ElevenForum or TenForums Quick Launch thread draws a steady
trickle of readers for years, because people keep searching `quick launch windows 11` and land on it. The
Microsoft Answers threads rank for the same reason. Expect single-digit to low-tens installs per thread
over its lifetime, plus backlinks that feed action 5.

Note that thread 1 (ElevenForum, Quick Launch) and thread 5 (TenForums, same question) are the same
question on two sites that index separately. Do both.

**Impact:** 10 threads x single-digit-to-low-tens = roughly 50 to 250 installs over months, plus the
backlink trail. Best effort-to-install ratio in the plan.

### 7. Post the r/windowsapps Show-and-tell, then comment in r/software

`REDDIT_POST_r_windowsapps.md` has been written since 7 September and never posted. FanFolder has zero
Reddit mentions, so the whole surface is open. r/windowsapps takes "I built X" posts from solo devs and
returns real feedback. Post it weekday morning US time with a GIF or short screen recording.

Two things to fix in the draft first: it says "grab the MSI from the latest release", which is fine, but
check the app name and version still match, and it opens on the title hook, which is right.

Then add a one-liner to the recurring utility threads in r/software (for example "My list of Must Have
Windows/Mac Utilities"). Comment culture there tolerates a specific, disclosed mention.

Do not full-post to r/windows or r/Windows11. Both remove raw self-promotion. Keep answering threads
there instead.

**Impact:** one post, unpredictable, with a real ceiling in the hundreds if it lands.

---

## Tier 2: borrow demand that already exists

### 8. Submit to AlternativeTo, MajorGeeks and FossHub

- **AlternativeTo.** Not listed yet (`ALTERNATIVETO_LISTING.md` is a draft). The placement that matters is
  as an *alternative to ExplorerPatcher, Start11, StartAllBack and RocketDock*, because those pages rank
  for exactly the problem searches above. Several taskbar utilities on those pages are flagged
  Discontinued, and each is a search slot going cold.
- **MajorGeeks.** Actively lists small Win32 utilities, its pages rank for taskbar-plus-utility searches.
- **FossHub.** Cheap, clean, one submission.

**Impact:** evergreen trickle, roughly 5 to 30 installs a month each once indexed. Never spikes, never
stops.

### 9. Build a real social card, then pitch creators and press — CARD DONE, PITCHES NOT SENT

Your `og:image` was `icon.png`, a square app icon. Every link you shared on Reddit, HN or a forum rendered a
square icon in the preview, and link previews with an app icon get a fraction of the clicks of one with a
real image.

Done 3 October 2026 (Pages commit `81a941d`):

- `social-card.png` at the dist repo root, exactly 1200x630, flattened RGB, 140 KB. Real app art: the fan
  open over a Windows taskbar, with the Wordmark, the line "Your recent files, one click away", and three
  chips.
- `og:image`, `twitter:image`, `og:image:width`, `og:image:height`, and alt text on the homepage and all
  four guides. Each guide has its own alt text.
- `twitter:card` changed from `summary` to `summary_large_image`. This was a second bug: `summary` renders a
  small thumbnail and would have ignored the card even after the image was correct.
- `scripts/make_social_card.py` and `assets/app-frame.png` are in the repo. Regenerating reproduces the
  deployed PNG byte for byte.

Still to do: the pitches. The list is in `PROMOTION_IDEAS.md`: XDA, Windows Central, Neowin, and the YouTube
channels that make "make Windows 11 look like macOS" videos. Offer three facts and a GIF, not a press
release. The hook that works is the XDA story: Windows *might* get the dock animation, and the useful
half of it is already on the taskbar today, in 1.4 MB, free.

**Impact:** the card multiplies the click rate on everything in actions 6, 7 and 11. The pitches
themselves are one-off spikes, probably 10 to 100 installs each if they land.

### 10. Decide whether to open-source it (a decision only you can make)

This is the biggest single reach multiplier in the plan, and it is a fork in the road rather than a task.

The license is proprietary with source withheld (README and LICENSE.md both say so plainly), and the
winget manifest declares `License: Proprietary`. That is a legitimate choice and I am not telling you to
change it. But be clear-eyed about what it costs:

- On GitHub, HN and the developer half of Reddit, closed-source freeware gets skipped by a large share of
  people. It is the single most common reason a thread replies "not open source?" and moves on.
- Three stars in six months is what a closed repo gets. Open repos in this category pick up stars, forks
  and issue traffic passively.
- GitHub topic search and any "best free Windows utilities" roundup mostly assume a public repo.
- You keep all of it either way if you use a source-available licence (e.g. PolyForm Noncommercial,
- BUSL, or Apache 2.0 with a trademark carve-out) or publish after a delay.

If you open it, actions 7 and 11 get materially stronger. If you keep it closed, spend the effort on
actions 5, 6 and 8 instead, which do not care.

**Impact:** not quantifiable. It is the difference between being findable by search and being findable
only by people who already found you.

---

## Tier 3: one-off spikes, do them once

### 11. Show HN

Only worth doing after action 10 is settled. If the repo stays closed, the top comment will be "why is
this not open source?" and that sets the tone for the thread. If it opens, lead with the native Win32/C++
engineering: 1.4 MB, no runtime, reads Jump Lists directly, does not patch explorer.exe. Keep marketing
words out of the post body. Zero prior mentions, so the slot is genuinely open.

### 12. Product Hunt

`installer/store/PRODUCTHUNT.md` is drafted and unposted. Be realistic: PH is weak for a Windows tray
utility with no web component. Do it once, on a Tuesday or Wednesday morning US time, with the Store
badge, and treat it as a one-day curiosity rather than an install source. Fix the wrong winget command in
it first (action 1).

---

## Ongoing

### 13. Pair every release with one thread answer and one Reddit comment

You have shipped 17 releases in six months and zero marketing touches. Release days are the only moments
you reliably have someone's attention. Ship, then spend 30 minutes: one forum reply, one Reddit comment,
one Store "what's new" that names the user-visible change in the first sentence.

### 14. The two-hour monthly loop

Once the templates exist: pull new thread titles for the five key phrases (`quick launch windows 11`,
`taskbar toolbars`, `recent files taskbar`, `jump list windows 11`, `mac dock on windows`), add or update
one guide page for anything repeating, and answer the new threads with a link to the matching page. Two
hours a month. This is what turns actions 5 and 6 from a campaign into a channel.

---

## First 48 hours, in order

Done 3 October 2026: action 1 (docs), action 2 (generator tags), action 3 (Store CSV), action 4 (dev tags),
action 5 (four guide pages live).
Left to do:

- [ ] Import the updated `FanFolderListing.csv` in Partner Center and submit (action 3)
- [ ] Ask five people for a Store rating (action 3)
- [ ] Post the r/windowsapps draft (action 7)
- [ ] Answer threads 1 and 5, the two Quick Launch threads (action 6), linking the new
      `/guides/quick-launch-windows-11/` page
- [x] Make the 1200x630 social card and repoint `og:image` (action 9, card half)
- [ ] Send the creator and press pitches (action 9, pitch half)
- [ ] Decide on open source (action 10)

Then: answer the remaining eight threads over the following fortnight.

## How to tell if it is working

GitHub release asset downloads are the only number that counts installs across winget, GitHub and direct
downloads, and they currently run about 5 a day. Store installs are invisible to that count, so check the
Partner Center dashboard separately. Watch three numbers monthly: release asset downloads, Store rating
count, and `winget search FanFolder` placement. If downloads have not moved off 5 a day after the Tier 0
and Tier 1 work, the guides are not ranking and the pages need to be rewritten around the exact query
phrases rather than around the product.
