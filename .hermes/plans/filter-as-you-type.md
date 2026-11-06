# Plan: Filter-as-you-type in the open fan

Branch: `filter-as-you-type`

## Goal
When a fan is open, typing live-filters the visible items by name. Escape clears
the filter (a second Escape closes the fan, preserving today's behavior). Enter
launches the top match. Turns the fan into a keyboard launcher for that folder
without leaving the arc metaphor.

## Key architecture findings (from reading the code)

- **The fan window is `WS_EX_NOACTIVATE`** (`FanWindow::Create`, FanWindow.cpp:165)
  — it deliberately never takes keyboard focus. So we CANNOT rely on `WM_CHAR`
  arriving at the fan's WndProc. This is exactly the "focus on a topmost window
  is fiddly" pitfall the task warns about.
- **Keyboard is already captured** by a low-level hook running on a dedicated
  thread in MainWindow: `KeyboardHookProc` (MainWindow.cpp:829), installed via
  `WH_KEYBOARD_LL` in `InstallHooks` (MainWindow.cpp:752). Today it only reacts
  to `VK_ESCAPE` and posts `WM_MAIN_CLOSE_FAN`. **This hook is the injection
  point** — it already sees every keystroke system-wide while the fan is open,
  which sidesteps the focus problem entirely. No `SetFocus`/foreground juggling.
- **Filtering engine already exists**: `FileService::ScanFolder(..., filterRegex, ...)`
  compiles a cached `std::wregex` (icase) against filenames (FileService.cpp:1043).
  But re-scanning the folder per keystroke is wrong (disk I/O, icon reload,
  re-animation). Instead we filter the already-loaded `_items` in the FanWindow
  in-memory. We reuse the *regex idea* (substring/regex match on name) but apply
  it to the in-memory list — "the regex filter you already support as the engine
  underneath" = same matching semantics, applied live.
- **Items & labels** live in `FanWindow::_items` + `_labelCache`. Display name
  comes from `ItemLabel(idx)` (FanWindow.cpp:990). Layout, hit rects, icon slots
  are all sized to `_items.size()` and rebuilt by `CalculateLayout()` /
  `RebuildLabelCache()`.
- **Placeholder path** already exists for empty lists (`_isPlaceholder`,
  FanWindow.cpp:129) — reuse the same "no items" rendering when a filter matches
  nothing.
- `LaunchItem(idx)` (FanWindow.cpp:1152) is the launch entry point; `HitTest`
  drives mouse launches. Enter should call `LaunchItem` on the top visible match.

## Design decision: filter a view, don't re-scan

Rather than mutate `_items` (which would fight the icon-load / prewarm / animation
machinery that all index into parallel vectors), introduce a **filter view**:

- Keep `_items` and all parallel vectors (`_bitmaps`, `_gdiBitmaps`, `_iconLoaded`,
  `_hoverScale`, etc.) intact and full-length — icons stay loaded once.
- Add `_filterText` (std::wstring) and `_visible` (std::vector<int>) mapping
  visible-slot → real item index. When `_filterText` is empty, `_visible` is the
  identity `[0..n)` (+ explorer button slot).
- Recompute layout over the *visible* set only, so the arc tightens to matches.

This is more surgery in the draw/layout/hit-test paths but keeps icons cached and
avoids per-keystroke disk work. **Prototype the focus/hook model first** (see
Phase 0) before committing to the full view-remap.

## Matching semantics

- Case-insensitive substring match of the typed text against the item's display
  name (`ItemLabel`) is the baseline (fast, predictable as a launcher).
- Treat the typed text as a literal by default; optionally, if it fails to
  compile or we want power-user regex, fall back to `std::wregex` icase like
  ScanFolder does. Start with substring (simpler, matches "launcher" feel);
  document regex as a possible follow-up. Reuse a cached compiled matcher to
  avoid recompiling every keystroke.

## Phases

### Phase 0 — Prototype the focus/hook model (de-risk first)
1. In `KeyboardHookProc`, while `_fanOpen`, forward printable keydowns and
   Backspace/Enter/Escape to the fan via a new posted message
   (e.g. `WM_FAN_FILTER_KEY` to `_fanWindow->Handle()`), instead of only Escape.
   - Use `MapVirtualKey` / `ToUnicodeEx` in the hook, OR post the raw VK and
     translate in the fan. Prefer posting raw VK + scan code and translating in
     the fan's WndProc to keep the hook cheap and layout-aware.
   - CRITICAL: return non-zero from the hook (`return 1;` — swallow) for keys we
     consume so the keystroke doesn't leak to whatever window is focused
     underneath. Escape/Enter/printables while filtering must be eaten.
2. Verify (CI build + manual on Windows) that keystrokes reach the fan and are
   swallowed from the underlying app. This is the fiddly part — confirm before
   building UI.

### Phase 1 — Filter state + view remap in FanWindow
3. Add members: `std::wstring _filterText;` `std::vector<int> _visible;`
   `bool _filterActive = false;`.
4. Add `void SetFilter(const std::wstring&)` that:
   - Rebuilds `_visible` from `_items` using the matcher.
   - Calls `RebuildLabelCache()` semantics for visible set, `CalculateLayout()`,
     resizes/reindexes only the *view* (not the icon vectors), and redraws.
   - Resets/keeps `_hoverIdx` sensibly (top match highlighted).
5. Rework `CalculateLayout`, `DrawToLayeredWindow`/`DrawItem`, `HitTest`,
   `LaunchItem`, and the animation tick to iterate over `_visible` and map back
   to real indices for icon/label lookups. (This is the bulk of the work — every
   `for i in items` in draw/layout/hit becomes "for slot in _visible".)
6. Empty match set → reuse `_isPlaceholder` rendering ("no matches" label; add a
   localized string alongside `EmptyFolderLabel()` in Localization).

### Phase 2 — Filter overlay UI
7. Draw a small text-capture overlay (the typed string + a caret) near the arc
   hinge, only when `_filterActive && !_filterText.empty()`. Reuse
   `DrawLabelPill` styling for visual consistency.
8. Show a subtle hint or highlight on the top match (the Enter target).

### Phase 3 — Key handling in the fan
9. In FanWindow WndProc handle `WM_FAN_FILTER_KEY`:
   - Printable char → append to `_filterText`, `SetFilter`.
   - Backspace → pop last char, `SetFilter`.
   - Escape → if `_filterText` non-empty: clear filter (stay open); else fall
     through to existing close behavior. NOTE: Escape close is currently decided
     in the hook (MainWindow) — move the "clear vs close" decision so that when a
     filter is active the hook does NOT post `WM_MAIN_CLOSE_FAN`. Simplest: hook
     always forwards Escape to the fan; the fan decides clear-or-close and, to
     close, posts `WM_MAIN_CLOSE_FAN` to the owner itself.
   - Enter → `LaunchItem(_visible.empty() ? -1 : _visible[0])` (top match), guard
     against placeholder.
10. Keep the mouse hooks / winevent close behavior unchanged.

### Phase 4 — Config + polish
11. Optional config toggle `filterAsYouType` (default on) in Config.h/Config.cpp,
    wired like existing bools, so it can be disabled. Check existing config
    plumbing before adding.
12. Reset `_filterText` on every fresh `Show()` so a reopened fan starts unfiltered.

## Files to touch
- `FanFolder/src/MainWindow.cpp` — extend `KeyboardHookProc` to forward keys +
  swallow; keep gen-stamping. Possibly new message constant.
- `FanFolder/src/FanWindow.h` / `FanWindow.cpp` — filter state, view remap,
  overlay draw, key handling. Bulk of the change.
- `FanFolder/src/Localization.h` / `Localization.cpp` — "no matches" string.
- `FanFolder/src/Config.h` / `Config.cpp` — optional toggle.

## Verification & local Windows testing (this is the test loop)

Cannot compile on WSL (needs MSVC + Windows SDK). The real x64 `FanFolder.exe` is
built by GitHub Actions and pulled down to replace the locally running exe. The
full, maintained procedure lives in the **`fanfolder-update-local-exe` skill,
scenario B** ("build & test a feature-branch version") — load it and follow it
rather than duplicating the commands here. It covers: dispatching CI on the
branch, polling the run, downloading the `build-x64` artifact, killing the
running exe (file lock), swapping + `md5sum` verify, and relaunch.

Condensed loop (per iteration):

1. `git push -u origin filter-as-you-type`
2. `gh workflow run release.yml --ref filter-as-you-type` (workflow_dispatch —
   builds on any branch, no tag/release needed).
3. Poll `gh run view <RUN_ID>` until `Build FanFolder (x64)` is `success`. **A
   green x64 build IS the compile check** (there is no local build). On failure:
   `gh run view <RUN_ID> --log-failed` → fix → re-push.
4. Download the artifact + swap the local exe per the skill's back-up/taskkill/
   copy/verify/relaunch steps. Keep the `.bak-*` copy for instant rollback.

### Manual acceptance checklist (open the fan from the taskbar, then)
- Type text → items live-filter by name; arc tightens to matches.
- Overlay shows the typed string + caret.
- Backspace edits the filter.
- Escape with text → clears filter (fan stays open); Escape with empty filter →
  closes fan (today's behavior).
- Enter → launches the top match.
- CRITICAL: typed keystrokes do NOT leak to the app underneath (the LL hook must
  swallow consumed keys).
- Filter resets when the fan is reopened.

Notes:
- A `workflow_dispatch` run also publishes a throwaway `dev-<sha>` PRE-RELEASE to
  the DIST repo — harmless; clean up with
  `gh release delete dev-<sha> --repo olebhartvigsen/FanFolder --cleanup-tag`.
- Do NOT bump version / edit `app.rc` / cut a real `vX.Y.Z` release on this
  branch — that's the separate release flow (`fanfolder-release-winget` skill).

## Next steps (suggested order)

1. **Confirm test-machine assumptions** (blocks the swap step): local install
   path is `C:\projekter\win-dir-fan\FanFolder\build\Release\FanFolder.exe` and
   arch is **x64** (not ARM). If either differs, adjust the swap/download.
2. **Phase 0 — prototype the hook/focus model.** This is the one genuinely risky
   piece (the task's explicit watch-out). Implement just the key-forwarding +
   swallow in `KeyboardHookProc` → `WM_FAN_FILTER_KEY`, plus a throwaway visible
   proof (e.g. type → show the captured string in the overlay / debug log).
   Push, CI-build, swap, and confirm on Windows that keystrokes reach the fan AND
   don't leak to the app underneath. **Do not build Phases 1–4 until this is
   proven.**
3. **Phase 1 — filter view-remap.** The bulk of the work; only worth doing once
   Phase 0 confirms the input path.
4. **Phases 2–4 — overlay UI, key semantics, config toggle + reset-on-Show.**
5. **Open a PR** from `filter-as-you-type` so CI runs on every push and the
   change is reviewable; keep iterating via the test loop above.

Recommended immediate action: answer step 1's two questions, then I start Phase 0.

## Watch-outs (carried from task + code)
- Focus on the layered topmost `WS_EX_NOACTIVATE` window is the trap — solved by
  routing through the existing LL keyboard hook, NOT by grabbing focus.
- Must swallow consumed keys in the hook or they leak to the focused app.
- Parallel per-item vectors (icons/anim) are indexed by real item index — the
  view remap must map visible-slot → real index everywhere, or icons/anim break.
- Re-scanning the folder per keystroke is wrong; filter the in-memory list.
- Layout sentinels (`FLT_MAX`) assume total>0 — keep the placeholder path for the
  empty-match case.
