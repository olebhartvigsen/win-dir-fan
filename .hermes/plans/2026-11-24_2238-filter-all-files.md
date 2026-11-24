# Filter across all files, applied as you type (FanFolder)

> **For Hermes:** This plan is for `/home/hermes/workspace/win-dir-fan` (repo `olebhartvigsen/win-dir-fan`). Base commit `01c744c` on `master`. Line numbers shift during implementation.

**Goal:** Typing in an open fan filters the icons against every file the selected folder can reach, not only the `MaxItems` slice that happened to be rendered. Each keystroke updates the result immediately, and the full search runs off the UI thread.

**Assumption behind the request:** "non displayed files" means files that exist in the folder but are outside the current `MaxItems` window, plus files in subfolders when `IncludeSubfolderFiles` is on. Full-text search inside documents is out of scope.

**Tech stack:** C++20, Win32, CMake, GitHub Actions (`windows-2022`).

---

## Current state (verified by reading the code)

Filter-as-you-type already exists in a first form. It is not the feature being asked for, but it is the right base.

- `ConfigData::filterAsYouType` (default `true`), `Config.h:11`, loaded and saved in `Config.cpp`. There is no tray-menu toggle and no README section for it.
- `MainWindow`'s low-level keyboard hook (`MainWindow.cpp:1204-1285`) translates keystrokes with `ToUnicodeEx` and posts `FanWindow::WM_FAN_FILTER_KEY` with a character, `VK_BACK`, `VK_RETURN` or `VK_ESCAPE`. It swallows the key only when the fan is visible and the setting is on.
- `FanWindow::ApplyFilter()` (`FanWindow.cpp:384`) rebuilds `_visible` from `_items` with a case-insensitive substring match on the label, then re-lays out and redraws.
- The search space is `_items`, and `_items` was already truncated to `MaxItems` by `FileService::ScanFolder` (`FileService.cpp:1283`). Files beyond the cap are invisible to the filter.
- To compensate, `ApplyFilter` runs a synchronous full scan with a 10,000 item cap on the UI thread when matches are few and the list is at the cap (`FanWindow.cpp:429-458`). On a large folder this blocks the UI thread for the duration of a full directory walk plus `.lnk` resolution, once per keystroke burst.
- Recent modes are worse: `ScanRecentDocs` (`FileService.cpp:784`), `ScanRecentFiles` (`FileService.cpp:915`) and the `::GraphRecent::` branch (`FileService.cpp:1173`) all cap their result at `maxItems` before the filter sees anything.
- The only filter UI is a pill drawn over the fan (`FanWindow.cpp:1101-1118`) showing `⌨ <text>_`, with a hardcoded U+2328 glyph, no placeholder, no match count, and no localized strings. `Localization.cpp` has no filter strings at all.
- `_iconGen` already drops stale async icon loads (`FanWindow.cpp:392`, `437`, `525`, `2185`), and `ReplaceItems` already preserves an active filter across a watcher refresh.
- The handoff pattern for posted results already exists twice (`IconHandoff`, `PrewarmHandoff`): register the payload under a token, post the token, resolve it on the UI thread. Any new cross-thread result must use the same pattern, because the window classes are findable by name.

## Behavior spec

| Case | Result |
|---|---|
| Fan opens, no text typed | Today's behavior, unchanged |
| First character typed | In-memory matches appear immediately from the items already loaded, no disk I/O |
| Typing continues | 120 ms after the last keystroke, a full search runs and replaces the view with complete results |
| Folder has 4,000 files, `MaxItems` 15 | Matches outside the first 15 appear. Pill shows `3 of 27` |
| More matches than `MaxItems` | The first `MaxItems` are drawn, ranked, and the pill shows the total |
| No match | Single non-interactive pill: the query, "no matches", and the folder name |
| Backspace until empty | `_originalItems` restored, icons restored, no search runs |
| Escape with text | Clears the text, fan stays open |
| Escape with no text | Closes the fan, as today |
| Enter | Launches the top-ranked match |
| Virtual folder (`::RecentDocs::`, `::RecentFiles::`, `::GraphRecent::`) | Searches a candidate set widened to 200 items at scan time, then filters in memory |
| `IncludeSubfolderFiles` off | Search covers the root folder only |
| `IncludeSubfolderFiles` on | Search covers subfolders to depth 8 |
| Folder changed by the watcher while filtered | `ReplaceItems` updates `_originalItems` and re-runs the search |
| File dropped on the fan while filtered | Existing behavior: filter clears, list rescans |
| Search still running when the fan closes | Result is discarded. Token freed, no touch of a dead `this` |
| Query longer than 64 characters | Ignored past 64 |
| IME composition (Japanese, Chinese, Korean) | Not supported by the LL hook. Documented limitation, see Risks |

## Design decisions

1. **Two tiers, one matcher.** Keep the instant in-memory pass for latency, add an async full search for completeness. Both call the same `FileService::MatchScore` so ranking never disagrees between the two.
2. **No disk I/O on the UI thread.** Delete the synchronous 10,000 item scan branch. The UI thread only ever filters a vector it already holds.
3. **Debounce 120 ms, one search in flight.** The directory walk is the expensive part; per-keystroke scans would thrash the disk and the shell. A newer query always wins by generation number, and stale results are dropped on arrival.
4. **Ranking beats raw substring order.** Exact name, then name prefix, then word-boundary, then substring in the name, then a match only in the relative folder. Within one rank, the active sort mode decides. This is what makes Enter predictable.
5. **Match file name and relative folder, not the rendered label.** The label is derived from those two, and matching the raw name means `.docx` is findable even when `ShowExtensions` is off. `ItemLabel` stays a rendering concern.
6. **Cap drawn results at `MaxItems`.** The arc layout is built for about 15 icons. The pill carries the total so the user knows the list is cut.
7. **No new registry setting in v1.** `FilterAsYouType` is already the master switch. Add the missing tray toggle so it is discoverable, nothing else.
8. **Locale strings as standalone functions**, following `EmptyFolderLabel()`, so the 29 positional `Strings` tables stay untouched.

## Files to change

| File | Change |
|---|---|
| `FanFolder/src/FileService.h` | `SearchResultSet`, `SearchFolder`, `MatchScore` declarations |
| `FanFolder/src/FileService.cpp` | Shared `SortItems` helper, `MatchScore`, `SearchFolder`, candidate cap for recent modes |
| `FanFolder/src/FanWindow.h` | Search state, debounce timer id, `WM_FAN_SEARCH_RESULTS`, new private methods |
| `FanFolder/src/FanWindow.cpp` | Async search driver, result application, search pill, remove synchronous full scan, query cap |
| `FanFolder/src/MainWindow.cpp` | Tray toggle for `FilterAsYouType` |
| `FanFolder/src/Localization.h/.cpp` | `FilterHint`, `NoMatchesLabel`, `MatchCountLabel` |
| `FanFolder/CMakeLists.txt` | Optional test target, see Task 9 |
| `README.md` | Feature bullet and a `FilterAsYouType` section |
| `.github/workflows/verify.yml` (or the release workflow) | Run the new tests |

---

## Task 1: Shared sort helper and matcher in FileService

**Objective:** One place that decides ordering, one place that decides whether an item matches and how well.

**Files:** `FanFolder/src/FileService.h`, `FanFolder/src/FileService.cpp`

**Step 1:** Extract the sort switch from `ScanFolder` (`FileService.cpp:1230-1281`) into a file-local helper. No behavior change.

```cpp
namespace {
void SortItems(std::vector<FileItem>& items, ConfigData::SortMode sortMode) {
    switch (sortMode) {
    case ConfigData::SortMode::DateModifiedDesc: /* existing comparator */ break;
    case ConfigData::SortMode::DateModifiedAsc:  /* existing comparator */ break;
    case ConfigData::SortMode::NameAsc:          /* existing comparator */ break;
    case ConfigData::SortMode::NameDesc:         /* existing comparator */ break;
    case ConfigData::SortMode::DateCreatedDesc:  /* existing comparator */ break;
    case ConfigData::SortMode::DateCreatedAsc:   /* existing comparator */ break;
    }
}
} // namespace
```

`ScanFolder` then calls `SortItems(items, sortMode);` before its `maxItems` truncation.

**Step 2:** Add the matcher to the header so `FanWindow` can use it too.

```cpp
// 0 = no match. 1 = match only in the relative folder. 2 = substring in the
// file name. 3 = word boundary in the name. 4 = name starts with the query.
// 5 = name equals the query (ignoring a hidden extension).  Higher is better.
static int MatchScore(const FileItem& item, const std::wstring& lowerQuery);
```

Implementation notes:
- Lowercase a copy of `item.name` with `CharLowerBuffW`, same casing rule the existing code uses.
- Compare against `lowerQuery` (already lowercased once by the caller, not per item).
- Word boundary means the match starts at index 0 or after one of space, `-`, `_`, `.`, `(`, `[`.
- If the name does not match, test `item.relativeDir`. A folder-only match returns 1.
- Empty query returns 0 and callers must not treat that as a match.

**Verify:** the matcher is pure logic. Task 9 pins it with a test table.

---

## Task 2: `FileService::SearchFolder`

**Objective:** Return every matching item in the folder, not only the `MaxItems` slice, with a total count.

**Files:** `FanFolder/src/FileService.h`, `FanFolder/src/FileService.cpp`

```cpp
struct SearchResultSet {
    std::vector<FileItem> items;      // ranked, sorted, truncated to maxResults
    int  totalMatches = 0;            // matches before truncation
    bool candidateCapHit = false;     // walk stopped at 20000 items
};

// Searches the same item set ScanFolder can see, ignoring maxItems for the
// candidate walk. Dispatches on the virtual sentinels like ScanFolder does.
static SearchResultSet SearchFolder(
    const std::wstring& folderPath,
    const std::wstring& query,
    int maxResults,
    bool includeDirs,
    const std::wstring& filterRegex,
    ConfigData::SortMode sortMode,
    bool includeSubfolderFiles);
```

Implementation:
1. For a real folder: reuse `CollectDirectory` with `kMaxRecursiveItems` (20,000) as the walk cap. Do not truncate to `maxItems` before matching. `pause` note: `CollectDirectory` already enforces the cap itself.
2. For `::RecentDocs::` and `::RecentFiles::`: call the existing scanners with `candidateCap = std::max(maxItems, 200)` instead of `maxItems`. This is a signature change on the two functions plus their `ScanFolder` call sites.
3. For `::GraphRecent::`: same widening, then the existing Office-extension filter, then matching.
4. Apply the existing `filterRegex` first, as `ScanFolder` does. The regex is a scan-level filter, the typed query is a view-level filter, and both can be active.
5. Lowercase the query once. Score every candidate with `MatchScore`, drop zeros, set `totalMatches`.
6. `std::stable_sort` by score descending, then by the active sort mode (reuse `SortItems` on each score bucket, or sort by a `(score, sortKey)` pair).
7. Truncate to `maxResults` and return. Set `candidateCapHit` when the walk hit `kMaxRecursiveItems`.

**Cost:** one directory walk per debounced search. On a 4,000 file folder this is a few milliseconds; `.lnk` resolution dominates when shortcuts are present. Measure before optimizing.

---

## Task 3: Search state in FanWindow

**Objective:** Own the query, the debounce, the generation counter, and the result handoff.

**Files:** `FanFolder/src/FanWindow.h`, `FanFolder/src/FanWindow.cpp`

**Step 1:** Messages and limits.

```cpp
// FanWindow.h, next to WM_FAN_FILTER_KEY
static constexpr UINT WM_FAN_SEARCH_RESULTS = WM_APP + 5;
```

`WM_ICON_READY` is `WM_APP+2`, `WM_FAN_FILTER_KEY` is `WM_APP+3`, and MainWindow already uses `WM_APP+1` and `WM_APP+4`. Keep this one unique and in the `WM_APP` range, because the payload is a token that leads to an owned allocation.

```cpp
// FanWindow.cpp, file scope
static constexpr UINT_PTR TIMER_ID_SEARCH       = 10;
static constexpr UINT     SEARCH_DEBOUNCE_MS    = 120;
static constexpr size_t   kMaxQueryLength       = 64;
static constexpr int      kRecentCandidateCap   = 200;
```

**Step 2:** Members.

```cpp
// Search state. _searchGen invalidates in-flight workers; _searchInflight
// keeps at most one directory walk running at a time.
std::atomic<int>  _searchGen{0};
// Shared, not a plain member: an in-flight walk may outlive the window, and a
// raw pointer to a member would dangle. The worker holds its own reference.
std::shared_ptr<std::atomic<bool>> _searchInflight =
    std::make_shared<std::atomic<bool>>(false);
int               _searchTotal   = 0;     // matches before truncation
bool              _searchRunning = false; // draw the "searching" state
bool              _searchPending = false; // query changed while a walk was running
```

The work item then carries the shared pointer:

```cpp
struct SearchWork {
    HWND        hwnd;
    ConfigData  cfg;
    std::wstring query;
    int         gen;
    int         maxResults;
    std::shared_ptr<std::atomic<bool>> inflight;
};
```

**Step 3:** Handoff. Copy the `IconHandoff` shape into a `SearchHandoff` namespace: `std::mutex`, `std::unordered_map<UINT, SearchPayload*>`, token counter, `Register`, `Take`, `Forget`. Payload:

```cpp
struct SearchPayload {
    std::vector<FileItem> items;
    int    total = 0;
    int    gen   = 0;
    std::wstring query;
};
```

**Step 4:** Debounce and submit.

```cpp
void FanWindow::ScheduleSearch() {
    if (!_hwnd) return;
    if (_filterText.empty()) { KillTimer(_hwnd, TIMER_ID_SEARCH); return; }
    SetTimer(_hwnd, TIMER_ID_SEARCH, SEARCH_DEBOUNCE_MS, nullptr);
}

void FanWindow::StartSearch() {
    if (_filterText.empty()) return;
    if (_searchInflight->load(std::memory_order_acquire)) {
        // One walk at a time. Remember that the query moved on, and start
        // again when the current result lands, so the last keystroke always
        // gets its own search.
        _searchPending = true;
        return;
    }

    const int gen = _searchGen.fetch_add(1, std::memory_order_acq_rel) + 1;
    _searchRunning = true;
    _searchPending = false;
    DrawToLayeredWindow();

    // Snapshot everything the worker needs. No member access on the worker,
    // because the fan window can be destroyed while the walk runs.
    auto* work = new SearchWork{ _hwnd, _config, _filterText, gen,
                                 std::max(_config.maxItems, 1),
                                 _searchInflight };
    _searchInflight->store(true, std::memory_order_release);

    TrySubmitThreadpoolCallback([](PTP_CALLBACK_INSTANCE, PVOID ctx) {
        std::unique_ptr<SearchWork> owner(static_cast<SearchWork*>(ctx));
        SearchWork* w = owner.get();

        // Balanced flag reset on the normal path and on the throw path.
        struct InflightGuard {
            std::atomic<bool>* flag;
            ~InflightGuard() { flag->store(false, std::memory_order_release); }
        } guard{ w->inflight.get() };

        try {
            auto set = FileService::SearchFolder(w->cfg.folderPath, w->query,
                                                 w->maxResults, w->cfg.includeDirs,
                                                 w->cfg.filterRegex, w->cfg.sortMode,
                                                 w->cfg.includeSubfolderFiles);
            auto* payload = new SearchPayload{ std::move(set.items), set.totalMatches,
                                               w->gen, w->query };
            const UINT token = SearchHandoff::Register(payload);
            if (!token || !PostMessageW(w->hwnd, WM_FAN_SEARCH_RESULTS, 0, (LPARAM)token)) {
                SearchHandoff::Forget(token);
                delete payload;
            }
        } catch (...) {
            // A failed search must never take the app down.
        }
    }, work, nullptr);
}
```

`SearchWork` holds its own `shared_ptr` to the in-flight flag, so the flag outlives a closed fan window. A raw `this` is not safe here because the fan can be destroyed while the walk runs; the worker touches only the copied config, the HWND and that shared flag. `PostMessageW` to a destroyed HWND returns FALSE, which the code already handles by freeing the payload.

**Step 5:** `WM_TIMER` handling for `TIMER_ID_SEARCH` (`KillTimer`, then `StartSearch()`), and `WM_FAN_SEARCH_RESULTS`:

```cpp
case WM_FAN_SEARCH_RESULTS: {
    std::unique_ptr<SearchPayload> p(SearchHandoff::Take((UINT)lParam));
    if (!p) return 0;
    self->_searchRunning = false;
    const bool stale = (p->gen != self->_searchGen.load(std::memory_order_acquire) ||
                        p->query != self->_filterText);
    if (!stale)
        self->ApplySearchResults(std::move(p->items), p->total);
    // Start the follow-up even after a stale result: it may be the one that
    // cleared the in-flight flag for a query the user typed meanwhile.
    if (self->_searchPending) self->StartSearch();
    return 0;
}
```

**Step 6:** Cancel on close. In `Close()` and `~FanWindow()`, `KillTimer(_hwnd, TIMER_ID_SEARCH)` and bump `_searchGen` so late results are dropped. Do not wait for the worker.

---

## Task 4: Apply results

**Objective:** Swap the visible list to the search result set without leaking icons or replaying the entry animation.

**File:** `FanFolder/src/FanWindow.cpp`

```cpp
void FanWindow::ApplySearchResults(std::vector<FileItem>&& items, int total) {
    // The prewarm icon arrays are indexed by the old list; new indices do not
    // line up, so drop the prewarm copies and load lazily.
    _prewarmBitmaps.clear();
    _prewarmIcons.clear();
    _prewarmGdiBitmaps.clear();
    _prewarmIconSize = 0;

    _iconGen.fetch_add(1, std::memory_order_release);  // drop in-flight loads

    _items = std::move(items);
    _isPlaceholder = (_items.empty());
    if (_isPlaceholder) {
        FileItem ph;
        ph.name = NoMatchesLabel(_filterText);   // the pill renders the query
        _items.push_back(std::move(ph));
    }
    _searchTotal = total;

    RebuildItemCaches();
    const int totalSlots = TotalSlots();
    _entryProgress.assign(totalSlots, 1.f);   // fan is already open, no replay
    _hoverScale.assign(totalSlots, 1.f);

    _visible.clear();
    for (int i = 0; i < (int)_items.size(); i++) _visible.push_back(i);
    _noMatchesActive = _isPlaceholder;
    _hoverIdx = (_visible.empty() || _noMatchesActive) ? -1 : 0;

    CalculateLayout();
    if (!_isPlaceholder)
        for (int i = 0; i < (int)_items.size(); i++) StartIconLoad(i);

    DrawToLayeredWindow();
}
```

`_originalItems` is deliberately untouched, so clearing the filter still restores the pre-filter list.

**Watch out:** `RebuildItemCaches()` resizes `_bitmaps`, `_icons`, `_iconLoaded` and `_gdiBitmaps`. Read it before changing anything here; the drop handler (`FanWindow.cpp:1667-1705`) is the closest existing example of the same sequence.

---

## Task 5: Remove the synchronous full scan

**Objective:** Delete the UI-thread disk walk that the async path replaces.

**File:** `FanFolder/src/FanWindow.cpp:421-459`

Remove the `kFullScanThreshold`, `needFullScan` block and its `ScanFolder(..., 10000, ...)` call. In the same edit:

- Keep the in-memory pass. It is what makes the first keystroke instant.
- Keep the `_visible` cap to `_config.maxItems`.
- Keep `_noMatchesActive` for the moment before results arrive, but render it as "searching" while `_searchRunning` is true, and as "no matches" only after a result for the current query has landed.

The `_visible.size() <= 3` heuristic goes away with it.

---

## Task 6: The search pill

**Objective:** Make the filter visible and honest about what it is showing.

**File:** `FanFolder/src/FanWindow.cpp:1101-1118`, plus a small draw helper

Replace the `⌨ <text>_` string with a three-part pill drawn from the existing `DrawLabelPill` plus one new helper:

```
[ magnifier ] query|                12 of 340
```

- Magnifier: two `DrawEllipse`/`DrawLine` calls, or a `GraphicsPath` arc plus a handle. Do not depend on a font glyph; U+2328 renders as a box on some systems, which is what happens today.
- Text: `_filterText`, with a 1 px caret drawn only when the app has an animation tick to repaint it, otherwise a static caret is fine. No blinking timer; the fan already runs at 60 FPS while open.
- Counter: drawn only when `_searchTotal > _items.size()` or `_searchTotal > 0` and a search has landed. Format via `MatchCountLabel(shown, total)`.
- Empty query: no pill at all. The fan is not a text box; the pill appears only once the user types.
- "Searching": draw three trailing dots and keep the last result set on screen. Do not blank the arc.
- Width: `min(0.6 * _winWidth, measured text)`. Cap the query display at 32 characters with a leading ellipsis so the caret stays visible.
- Position: keep the current bottom-left-of-hinge placement. It is out of the arc's way.

Do not change the layout constants for the icons. The pill is an overlay, not a layout participant.

---

## Task 7: Input handling hardening

**File:** `FanWindow.cpp:2248-2281` (`WM_FAN_FILTER_KEY`), `MainWindow.cpp:1204-1285`

- Enforce `kMaxQueryLength` before `push_back`.
- Ignore a character if `_filterText.size() >= kMaxQueryLength`.
- Call `ScheduleSearch()` after every text mutation instead of `ApplyFilter()` directly. `ApplyFilter` still runs first for instant feedback, then the debounce starts.
- `VK_ESCAPE` with text: clear, `ApplyFilter()`, reset `_searchTotal = 0`, `KillTimer`.
- `VK_BACK` to empty: same reset path as Escape, including a `_searchGen` bump.
- `VK_RETURN`: unchanged, launches slot 0. Ranking now guarantees slot 0 is the best match rather than the first in sort order.
- `VK_DELETE` and Home/End/arrow navigation are out of scope; the query is append and backspace only.
- Document the IME gap in the README. The hook consumes `WM_KEYDOWN` and calls `ToUnicodeEx`, which cannot see composition strings from an IME. Fixing that needs a real edit control, which is a separate design.

---

## Task 8: Localization

**Objective:** Three strings, without touching the 29 positional tables.

**Files:** `FanFolder/src/Localization.h`, `FanFolder/src/Localization.cpp`

```cpp
// Shown inside the empty search pill before the user types (tray menu uses it
// too as the setting label).
const wchar_t* FilterAsYouTypeLabel();
// "No matches for \"%s\"". Caller passes the query.
const wchar_t* NoMatchesLabel(const std::wstring& query);
// "12 of 340". Built with std::to_wstring, no locale table needed.
const wchar_t* MatchCountLabel(int shown, int total);
```

Add the reviewed languages (da, sv, no, de, nl, fr, es, it) and let the rest fall back to English, following the guidance in the subfolder plan. Keep the file ASCII-only with `\uXXXX` escapes.

---

## Task 9: Tests for the matcher

**Objective:** Pin the ranking, the case folding, and the truncation, because those are the parts that will silently rot.

**Files:** `FanFolder/CMakeLists.txt`, new `FanFolder/tests/FileServiceMatchTests.cpp`

```cmake
option(FANFOLDER_BUILD_TESTS "Build the matcher tests" OFF)
if(FANFOLDER_BUILD_TESTS)
    add_executable(FanFolderTests tests/FileServiceMatchTests.cpp src/FileService.cpp)
    target_include_directories(FanFolderTests PRIVATE src resources)
    target_compile_definitions(FanFolderTests PRIVATE UNICODE _UNICODE)
    target_link_libraries(FanFolderTests PRIVATE shell32 shlwapi ole32 version)
    enable_testing()
    add_test(NAME FileServiceMatch COMMAND FanFolderTests)
endif()
```

No test framework. A `struct Case { const wchar_t* name; const wchar_t* dir; const wchar_t* query; int expected; }` table plus an `int failures` counter, printing each mismatch and returning non-zero. Cases:

| name | relativeDir | query | expected score |
|---|---|---|---|
| `rapport.docx` | `` | `rapp` | 4 (prefix) |
| `rapport.docx` | `` | `RAPP` | 4 (case-insensitive) |
| `skrapport.docx` | `` | `rapp` | 2 (substring, no boundary) |
| `min-rapport.docx` | `` | `rapp` | 3 (word boundary after `-`) |
| `rapport.docx` | `` | `docx` | 2 (substring in name) |
| `rapport.docx` | `Projekt A` | `projekt` | 1 (folder only) |
| `rapport.docx` | `` | `zzz` | 0 |
| `rapport.docx` | `` | `` | 0 |

Add the workflow step (`cmake -B build -DFANFOLDER_BUILD_TESTS=ON`, `ctest --test-dir build -C Release`) so the table is enforced in CI, not just run once.

---

## Task 10: Tray toggle and README

**Files:** `MainWindow.cpp` (menu enum at `857-875`, menu build around `950-967`, handler around `982`), `README.md`

- New `ID_FILTER_AS_YOU_TYPE`, checked state from `_config.filterAsYouType`, handler flips it, `changed = true` so the existing block saves and prewarms.
- Label from `FilterAsYouTypeLabel()`. Place it after `ShowExtensions`.
- README: feature bullet under the existing list (line 30 area) and a settings section after `FilterRegex` (`README.md:216`) documenting `FilterAsYouType`, the 64 character cap, the ranking order, and the IME limitation.

---

## Task 11: Verification

**Step 1: Compile check on this host.** No MSVC here. Use the mingw-w64 recipe in `.hermes/skills/fanfolder-build-check`:

```bash
cd FanFolder
for f in src/*.cpp; do
  x86_64-w64-mingw32-g++-posix -std=c++20 -fsyntax-only -DUNICODE -D_UNICODE \
    -I src -I resources -I /tmp/fancheck/stub "$f" 2>&1 | grep -c "error:"
done
```

Every file must report 0 before anything is pushed. This is a type check, not proof of behavior.

**Step 2: CI.** Branch, commit, push, run the workflow. Ask for permission first: `AGENTS.md` forbids commit and push without explicit approval.

**Step 3: Manual matrix on Windows.**

| Check | Expected |
|---|---|
| Empty folder, type `a` | "No matches" pill with the query, no crash |
| 4,000 file folder, `MaxItems` 15, type a name that is item 300 | Appears within about 200 ms |
| Type fast, 8 characters | One directory walk at the end, not eight |
| Hold a key down | No queue buildup, no UI freeze |
| Backspace to empty | Original 15 items return with their icons |
| Escape with text | Text clears, fan stays open |
| Escape with no text | Fan closes |
| Enter | Opens the top match, not the first alphabetically |
| Type `.pdf` with `ShowExtensions` off | PDFs match |
| Search in `::RecentFiles::` | Matches beyond the first 15 recents |
| `IncludeSubfolderFiles` on | Nested matches appear, label shows the relative folder |
| `IncludeSubfolderFiles` off | Nested files do not match |
| Add a file while filtered | Results refresh, filter preserved |
| Close the fan mid-search | No crash, no leaked token |
| Filter with a huge query (200 chars) | Capped at 64, no layout break |
| IME (if available) | Documented gap, no crash |

---

## Risks and tradeoffs

1. **Directory walk per debounce window.** 120 ms is a guess. Measure on a network share and a 20,000 file tree. If it hurts, raise the debounce and skip the search while the query is a single character.
2. **Two sources of truth during the wait.** The in-memory set and the full result set can differ for up to one walk. The pill's counter only appears after the search lands, so the user is never shown a wrong total.
3. **Icon churn.** Each applied result set loads icons for up to `MaxItems` new indices. Generation bumps keep stale loads from landing in the wrong slot, but a fast typist can queue several sets. Mitigation: only apply results whose query matches the current text, which the generation check already enforces.
4. **Recent modes are slower than they look.** Expanding the candidate cap to 200 re-parses jump lists. The debounce covers it, but the first search in `::RecentFiles::` may take longer than a local folder search.
5. **Ranking is a product decision.** Users may expect pure sort order. The counter and the top-match Enter behavior only feel right if ranking is visible, hence the score ordering in the plan.
6. **IME input does not work.** Real fix requires an edit control, which conflicts with the layered no-activate window design. Out of scope, documented.
7. **`MAX_PATH`.** `FindFirstFileW` fails past 260 characters, so deep subfolder searches inherit the existing limit.
8. **Locale review.** Machine translated strings on the Store are worse than English fallbacks.

## Open questions for the user

1. Should search also look in subfolders when `IncludeSubfolderFiles` is off? Recommendation: no, follow the setting. A separate "search whole tree while typing" toggle is a reasonable follow-up.
2. Should the fan draw more than `MaxItems` results when the query is narrow, for example up to 25? Recommendation: no in v1, keep the arc size and show the count.
3. Should a folder-name match rank above a file-name substring match? Recommendation: no, file name wins, folder is the lowest rank.
4. Should the query persist between opens? Recommendation: no. The fan opens clean.
5. Should `FilterAsYouType` default stay `true`? Recommendation: yes, it is already the shipped default.

## Execution note

This host has no MSVC, no `dotnet`, and no Windows SDK. The compile gate is the mingw-w64 cross-check described above, and the build gate is GitHub Actions on `windows-2022`. Runtime behavior can only be verified on the user's Windows machine. A green CI build proves compilation, not that the search feels fast. Do not commit or push without explicit permission.