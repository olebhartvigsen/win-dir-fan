# FanFolder: places to create attention

Where FanFolder stands (checked Sep 2026):

- Zero mentions on Reddit. Every post or comment is a first.
- Homepage is indexed and shows up in Google results for the space.
- winget, Microsoft Store, Product Hunt, HN are already planned in MARKETING.md. This file covers the interactive side: live threads, communities, and listings MARKETING.md does not have.

---

## 1. Live threads to answer

Ranked, newest and most promising first. For forum posts, disclose that you built FanFolder in the first comment. For threads older than ~6 months, a reply still helps whoever searches later, but expect little direct traffic.

| # | Place | Thread | What they want | Angle |
|---|-------|--------|----------------|-------|
| 1 | ElevenForum | [How do I get the Quick Launch bar back in Windows 11?](https://www.elevenforum.com/t/how-do-i-get-the-quick-launch-bar-back-in-windows-11.40154/) (Sep 24, 2025) | Quick Launch folders on Win 11, tried StartAllBack and ExplorerPatcher, still unhappy | FanFolder gives the folder popup from the tray without patching explorer.exe. No old-taskbar hacks, survives feature updates |
| 2 | ElevenForum | [Quick Launch with 24H2?](https://www.elevenforum.com/t/quick-launch-with-24h2.29681/) (multi-page, still active) | Toolbar workflow keeps breaking on 24H2+ | Same. Point out FanFolder is a plain app, so Windows updates cannot kill it |
| 3 | ElevenForum | [Win 11 Pro can't add specific folder to Taskbar quick launch](https://www.elevenforum.com/t/win-11-pro-cant-add-specific-folder-to-taskbar-quick-launch.28155/) | One specific folder on the taskbar | FanFolder watches any folder you point it at, recent files via Jump Lists also included |
| 4 | r/Windows11 | [Windows Explorer Add-Ons](https://www.reddit.com/r/Windows11/comments/1p403f8/windows_explorer_addons/) (~Oct 2025) | Toolbars with folder shortcuts in Explorer; QTTabBar broken on 25H2 | FanFolder sits next to the taskbar and covers the folder-list part. Mention it as one option among the answers |
| 5 | TenForums | [How do I get the Quick Launch bar back in Windows 11?](https://www.tenforums.com/general-support/221278-how-do-i-get-quick-launch-bar-back-windows-11-a.html) | Quick Launch on Win 11 | Same as ElevenForum. Cross-post the same reply, both forums share many users but still index separately |
| 6 | Microsoft Answers | [Windows 11 Pin Files Onto Taskbar Icon](https://answers.microsoft.com/en-us/windows/forum/all/windows-11-pin-files-onto-taskbar-icon/98fd2803-03f1-430f-a772-a09da56053b7) | Wants to reach files from the taskbar, drag and drop is blocked | FanFolder shows recent files on click and accepts dropped files too |
| 7 | Microsoft Answers | [Taskbar and toolbars in Windows 11](https://answers.microsoft.com/en-us/windows/forum/all/taskbar-and-toolbars-in-windows-11/ed5c0a61-b854-4046-958e-ba0271b28180) (updated Feb 2025) | Angry that toolbars are gone | Calm answer, same angle |
| 8 | r/Windows11 | [Has the taskbar toolbar been removed in Windows 11](https://www.reddit.com/r/Windows11/comments/1jlubqt/has_the_taskbar_toolbar_been_removed_in_windows_11/) (~spring 2025) | Confirmation + workaround | Answer the question, then mention the app briefly |
| 9 | r/Windows11 | [Did Windows 11 get rid of start menu recent documents per application?](https://www.reddit.com/r/Windows11/comments/1e2h5mm/did_windows_11_get_rid_of_start_menu_recent/) (Jul 2024, MS staff replied) | Jump lists per app from Start | FanFolder reads Windows Jump Lists directly, which is exactly the removed feature |
| 10 | r/WindowsHelp | [Windows 11 Cannot View Any Recent Files Anywhere](https://www.redditmedia.com/r/WindowsHelp/comments/1i7afex/windows_11_cannot_view_any_recent_files_anywhere/) (early 2025) | Recent files missing system-wide | Fix first (settings/policy), FanFolder as a way to see recent files regardless |

Older but evergreen (only worth a short reply, people still find these via Google):
- [r/Windows11: Taskbar quick folders](https://www.reddit.com/r/Windows11/comments/1bd1v1c/taskbar_quick_folders/). A commenter wrote "This should be possible with a third party app that shows tray icons. IDK if it exists yet." FanFolder is that app.
- [r/Windows11: How can I get Windows 11 to show recently used files?](https://www.reddit.com/r/Windows11/comments/193yjjc/how_can_i_get_windows_11_to_show_recently_used/)

---

## 2. Communities for a proper post

1. **r/windowsapps** (best first post). The sub runs a steady stream of "I built X" posts from solo devs: WindowSill, Sidekit, SHARA, Folio all posted there in 2026 and got real feedback threads. FanFolder fits the format exactly: small utility, screenshot or GIF, short story, download link. Post version 1.x with one clear hook (recent files on the taskbar, no ExplorerPatcher needed).
2. **r/software**. Watch and comment in their recurring "must have utilities" threads. Example: [My list of Must Have Windows/Mac Utilities for 2026](https://www.reddit.com/r/software/comments/1qi6zep/my_list_of_must_have_windowsmac_utilities_for_2026/). A one-line addition ("FanFolder, free, puts recent files on the taskbar") fits the comment culture there. Full self-posts are hit and miss in this sub.
3. **Hacker News** (Show HN). MARKETING.md already lists it. Verified that nothing about FanFolder is on HN yet, so the slot is open. The 161 KB native C++ angle plays well with that crowd; lead with the engineering, keep marketing out.
4. **ElevenForum / TenForums as a poster, not just a replier**. Both have General Support sections where third-party tools get recommended by regulars all the time. After answering a few threads as a normal helpful user, a short "third-party tool" intro post is acceptable there if you disclose authorship.
5. **r/Windows11** full post. Higher risk: the sub dislikes raw self-promotion and mods remove it. Safer there: keep answering the threads in section 1 until one gets traction.

---

## 3. Listings and slots MARKETING.md does not have

- **AlternativeTo**: FanFolder is not listed. Two angles:
  1. Add it as an alternative to **RocketDock, Start11, StartAllBack, ExplorerPatcher** (the tools people currently install just to get folders back on the taskbar).
  2. Several taskbar utilities on those pages are flagged Discontinued (TaskbarDock, Taskix, Taskbar++). Each is a search-result slot FanFolder can occupy.
- **MajorGeeks**: actively lists tiny Win32 utilities, and its pages rank well for "taskbar + utility" searches. Worth a submission.
- **YouTube micro-influencers**: there is a whole genre of "fix the Windows 11 taskbar" videos (quick launch tutorials, "this free app fixes Windows 11"). Channels in that niche review free utilities on request. A 30-second GIF demo (MARKETING.md item 1) is the entry ticket.

---

## 4. Reply templates

Adapt to the thread, do not paste identical text twice (Reddit and forums both fingerprint copy-paste).

Help thread (Quick Launch / toolbars gone):

> Windows 11 removed taskbar toolbars, which is why the option is gone. If you want them back, ExplorerPatcher restores the Windows 10 taskbar and its toolbars, StartAllBack does the same commercially. Both work by patching explorer.exe, so they can break on Windows updates. If you only need the folder and recent-files part, I wrote a small tray app called FanFolder: click the icon and your recent files (or any folder you pick) fan out from the taskbar, no shell patching. Full disclosure, I am the developer.

Recent files thread:

> The "show recently opened items" setting only works when the app writes Jump List entries, and some policies or Explorer bugs block it. A way around it: I built FanFolder, a free tray app that reads your Windows Jump Lists and shows recent files in one click. I am the developer, so take that with a grain of salt.

r/windowsapps post skeleton:

> I built a 161 KB taskbar app because I kept digging through Explorer for files I had just used. Click the tray icon and your recent files fan out; you can also point it at Downloads or a project folder. Native Win32/C++, no runtime, free. Windows 10/11. Looking for feedback, especially on the animation styles.

---

## 5. Rules of engagement

- Disclose authorship in the first comment everywhere. It reads as honest and prevents the "shill" reply.
- Lead with the fix for the actual question, app second. Threads reward answers, not pitches.
- Do not resurrect 2024 threads with a link dump. Short reply, link only if it fits.
- Reddit anonymous search is weak. For monitoring, register a free Reddit script app (reddit.com/prefs/apps) and set REDDIT_CLIENT_ID/REDDIT_SECRET, then the skill searches at full speed.
- A weekly pass over: Reddit (quick launch, toolbars, recent files, jump list), ElevenForum, TenForums, Microsoft Answers. Each takes minutes once the templates exist.
