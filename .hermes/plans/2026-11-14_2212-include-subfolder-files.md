# Include files from subfolders (FanFolder) implementation plan

> **For Hermes:** This plan is for `/workspace/win-dir-fan` (repo `olebhartvigsen/win-dir-fan`), not `/workspace/gudstjenste-scraper`. The gudstjeneste scraper reads a fixed `urls-to-scrape.txt` and has no folder picker or settings, so the request does not apply there.

**Goal:** Add an `IncludeSubfolderFiles` setting, off by default, that lets the fan list files found anywhere under the selected folder, not only files directly inside it.

**Architecture:** Keep the current flat scan as the default path. Add a recursive branch to `FileService::ScanFolder`, driven by a new `ConfigData` field read from `HKCU\SOFTWARE\FanFolder`. Add one tray-menu toggle. Extend the prewarm folder stamp so a change deep in the tree invalidates the cached scan. Nested items show their relative subfolder in the label so duplicate file names stay distinguishable.

**Tech stack:** C++20, Win32, CMake, GitHub Actions (`windows-2022`).

**Base commit:** `1629237` on `master`. Line numbers below are from that commit and will shift during implementation.

---

## Current state (verified by reading the code)

- `ConfigData` has `includeDirs` (`IncludeDirectories`, default true). `Config::Load` and `Config::Save` handle every field by hand.
- `FileService::ScanFolder` does one `FindFirstFileW` on `folderPath\*`, skips `.` entries and hidden/system entries, resolves `.lnk`, applies the regex filter, sorts, then truncates to `maxItems`.
- Three call sites:
  - `MainWindow.cpp:633` (prewarm worker)
  - `FanWindow.cpp:441` (full-folder scan during filter-as-you-type)
  - `FanWindow.cpp:1566` (rescan after a drop)
- `FileService::ReadFolderStamp` reads only the root folder's `ftLastWriteTime`. `MainWindow::StartPrewarmIfFolderChanged` trusts the cached prewarm when the config and stamp match, so a file added deep in the tree would not trigger a rescan.
- The only settings menu is `MainWindow::ShowTrayMenu`. It already computes `isRecentMode` to gray out date-created sort options for virtual folders.
- UI strings live in a positional `Strings` struct with 29 locale initializers. `EmptyFolderLabel()` is the sanctioned pattern for adding one label without editing all 29 tables.

## Behavior spec

| Case | Result |
|---|---|
| Setting off (default) | Exactly today's behavior. |
| Setting on, real folder | Root files plus files in every visible subfolder up to depth 8. |
| Setting on, virtual sentinel (`::RecentDocs::`, `::RecentFiles::`, `::GraphRecent::`) | No effect. Menu item is grayed out. |
| `IncludeDirectories` on | Root folders still appear as items. Nested folders are traversed but not listed as items. |
| `IncludeDirectories` off | No folders as items. Nested files are still included. |
| Duplicate leaf names | Label shows the relative path, for example `Projekt A\rapport.docx` (extension hidden unless `ShowExtensions` is on). |
| Junction or symlink loop | Not followed. No hang. |
| Hidden or system subfolder | Skipped entirely. |
| More than 20,000 files | Scan stops at the cap and writes a debug line. Safety valve, not a product limit. |
| Deeper than 8 levels | Not scanned. |

## Design decisions (recommended)

1. **Default off.** The feature changes what the fan shows, so it must be opt-in.
2. **Nested folders are not items.** The fan stays a file launcher. Root folders keep today's behavior. `IncludeDirectories` still controls only root folders.
3. **Relative folder in labels.** Two files named `rapport.docx` in different subfolders would otherwise look identical.
4. **Depth cap 8 and item cap 20,000.** Both are constants in `FileService.cpp`, not user settings. They protect the worker from pathological trees.
5. **Cache stamp walks directories, not files.** A max of directory mtimes catches add, remove, and rename anywhere in the tree, which matches what the root stamp guarantees today. In-place file edits still do not invalidate the cache, same as today.
6. **Separate locale function.** Follow `EmptyFolderLabel()` so the 29 positional `Strings` tables stay untouched.
7. **Registry key `IncludeSubfolderFiles`, not `IncludeSubfolders`.** The longer name avoids confusion with `IncludeDirectories`.

## Files to change

| File | Change |
|---|---|
| `FanFolder/src/Config.h` | Add `bool includeSubfolderFiles = false;` |
| `FanFolder/src/Config.cpp` | Load and save the new `REG_DWORD` |
| `FanFolder/src/FileService.h` | Add `relativeDir` to `FileItem`; add parameters to `ScanFolder` and `ReadFolderStamp` |
| `FanFolder/src/FileService.cpp` | Recursive walk, recursive stamp, sort tie-breakers |
| `FanFolder/src/MainWindow.cpp` | Menu item, handler, pass the flag to scan and stamp |
| `FanFolder/src/FanWindow.cpp` | Pass the flag at both scan sites; label prefix in `ItemLabel` |
| `FanFolder/src/Localization.h` | Declare `IncludeSubfolderFilesLabel()` |
| `FanFolder/src/Localization.cpp` | Implement the label for 29 locales |
| `README.md` | New settings section and feature bullet |
| `FanFolder/resources/app.rc`, `installer/msix/AppxManifest.xml` | Version bump only when cutting a release |

---

## Task 1: Config field and registry persistence

**Objective:** Persist the new setting and read it back on every launch.

**Files:**
- `FanFolder/src/Config.h:5-24`
- `FanFolder/src/Config.cpp:54-62` (load block after `IncludeDirectories`)
- `FanFolder/src/Config.cpp:201-204` (save block)

**Step 1: Add the field**

```cpp
struct ConfigData {
    std::wstring folderPath;
    int  maxItems       = 15;
    bool includeDirs    = true;
    bool includeSubfolderFiles = false;   // list files found in subfolders too
    bool showExtensions = false;
    // ...
};
```

**Step 2: Load it**

```cpp
// Config.cpp, Config::Load(), after the IncludeDirectories block
// IncludeSubfolderFiles (default: false)
{
    DWORD val = 0;
    DWORD size = sizeof(val);
    DWORD type = REG_DWORD;
    if (RegQueryValueExW(hKey, L"IncludeSubfolderFiles", nullptr, &type,
                         (LPBYTE)&val, &size) == ERROR_SUCCESS && type == REG_DWORD) {
        cfg.includeSubfolderFiles = (val != 0);
    }
}
```

**Step 3: Save it**

```cpp
// Config.cpp, Config::Save()
DWORD includeSubFiles = cfg.includeSubfolderFiles ? 1 : 0;
RegSetValueExW(hKey, L"IncludeSubfolderFiles", 0, REG_DWORD,
               (const BYTE*)&includeSubFiles, sizeof(includeSubFiles));
```

`ConfigData::operator==` is defaulted, so the new field automatically invalidates a cached prewarm when it changes. No hand-written compare to update.

**Verify:**

```powershell
reg query HKCU\SOFTWARE\FanFolder /v IncludeSubfolderFiles
```

Expected after toggling: type `REG_DWORD`, value `0x0` or `0x1`.

---

## Task 2: Extend `FileItem` and `ScanFolder`

**Objective:** Carry the relative subfolder with each item and add an explicit recursion flag.

**Files:**
- `FanFolder/src/FileService.h:6-13`
- `FanFolder/src/FileService.h:17-23`

```cpp
struct FileItem {
    std::wstring name;
    std::wstring fullPath;
    std::wstring relativeDir;   // "" at root, "Sub\Deeper" for nested files
    std::wstring targetPath;
    bool isDirectory = false;
    FILETIME lastWriteTime = {};
    FILETIME creationTime  = {};
};

static std::vector<FileItem> ScanFolder(
    const std::wstring& folderPath,
    int maxItems = 15,
    bool includeDirs = true,
    const std::wstring& filterRegex = L"",
    ConfigData::SortMode sortMode = ConfigData::SortMode::DateModifiedDesc,
    bool resolveLnk = true,
    bool includeSubfolderFiles = false);
```

The new parameter goes last so existing positional calls still compile. Update all three call sites explicitly anyway.

---

## Task 3: Recursive walk in `FileService.cpp`

**Objective:** Replace the single-level `do/while` loop with a directory queue. With recursion off, the queue holds only the root and behavior is unchanged.

**File:** `FanFolder/src/FileService.cpp:1039-1191`

**Step 1: Add the helper and constants immediately before `ReadFolderStamp` (line 1022)**

`ReadFolderStamp` (Task 4) and `ScanFolder` both use `DirJob` and the two constants, so this block must sit above both functions.

```cpp
namespace {

constexpr int    kMaxSubfolderDepth = 8;      // root = 0
constexpr size_t kMaxRecursiveItems = 20000;  // safety valve

struct DirJob {
    std::wstring path;
    std::wstring relDir;
    int depth;
};

// Enumerates one directory. Root directories are added as items when
// includeDirs is true. Non-reparse child directories are queued when
// recurse is true. Nested directories are never added as items.
void CollectDirectory(const std::wstring& dir, const std::wstring& relDir,
                      int depth, bool isRoot, bool includeDirs, bool resolveLnk,
                      bool recurse, std::vector<FileItem>& items,
                      std::vector<DirJob>& pending)
{
    std::wstring prefix = dir;
    if (!prefix.empty() && prefix.back() != L'\\') prefix += L'\\';

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW((prefix + L'*').c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (fd.cFileName[0] == L'.') continue;
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;

        const bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        // Never follow junctions or symlinked directories: cycles and duplicate trees.
        if (isDir && (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;

        if (isDir) {
            if (isRoot && includeDirs) {
                FileItem item;
                item.name = fd.cFileName;
                item.fullPath = prefix + fd.cFileName;
                item.isDirectory = true;
                item.lastWriteTime = fd.ftLastWriteTime;
                item.creationTime  = fd.ftCreationTime;
                items.push_back(std::move(item));
            }
            if (recurse && depth < kMaxSubfolderDepth) {
                std::wstring childRel = relDir.empty()
                    ? fd.cFileName
                    : relDir + L"\\" + fd.cFileName;
                pending.push_back({ prefix + fd.cFileName, std::move(childRel), depth + 1 });
            }
            continue;
        }

        FileItem item;
        item.name = fd.cFileName;
        item.fullPath = prefix + fd.cFileName;
        item.relativeDir = relDir;
        item.lastWriteTime = fd.ftLastWriteTime;
        item.creationTime  = fd.ftCreationTime;

        if (resolveLnk) {
            auto dot = item.name.rfind(L'.');
            if (dot != std::wstring::npos) {
                std::wstring ext = item.name.substr(dot);
                for (auto& c : ext) c = (wchar_t)towlower(c);
                if (ext == L".lnk") {
                    bool targetIsDir = false;
                    std::wstring target = FileService::ResolveLnk(item.fullPath, targetIsDir);
                    if (target.empty()) continue;
                    item.targetPath  = target;
                    item.isDirectory = targetIsDir;
                }
            }
        }

        if (!item.isDirectory || (isRoot && includeDirs))
            items.push_back(std::move(item));
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

} // namespace
```

**Step 2: Replace the flat enumeration in `ScanFolder`**

```cpp
std::vector<DirJob> pending;
pending.push_back({ folderPath, L"", 0 });

while (!pending.empty() && items.size() < kMaxRecursiveItems) {
    DirJob job = std::move(pending.back());
    pending.pop_back();
    CollectDirectory(job.path, job.relDir, job.depth, job.depth == 0,
                     includeDirs, resolveLnk, includeSubfolderFiles,
                     items, pending);
}

if (!pending.empty()) {
    OutputDebugStringW(L"[FanFolder] subfolder scan hit the item cap\n");
}
```

The old `prefix`, `FindFirstFileW`, `do/while`, and `FindClose` block goes away. The old `if (!includeDirs)` directory filter also goes away, because `CollectDirectory` only adds directories when `includeDirs` is true. The regex filter, sort, and `maxItems` truncation stay where they are.

**Step 3: Sort tie-breakers**

Name sorts currently compare only `name`. With nested files, two items can have the same leaf name, so the order is arbitrary. Add `fullPath` as the tie-breaker:

```cpp
case ConfigData::SortMode::NameAsc:
    std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
        int c = _wcsicmp(a.name.c_str(), b.name.c_str());
        if (c != 0) return c < 0;
        return _wcsicmp(a.fullPath.c_str(), b.fullPath.c_str()) < 0;
    });
    break;
```

Do the same for `NameDesc` (invert both comparisons) and for the date sorts (compare `fullPath` when the times are equal). This only changes ties.

**Verify by reading:** with `includeSubfolderFiles == false`, `pending` gets only the root job, `recurse` is false, and no child jobs are queued. The resulting item set is the same as the old loop.

---

## Task 4: Recursive folder stamp

**Objective:** Make the prewarm cache notice a file added, removed, or renamed anywhere in the tree.

**Files:**
- `FanFolder/src/FileService.h:49-56`
- `FanFolder/src/FileService.cpp:1022-1037`

Change the signature:

```cpp
static bool ReadFolderStamp(const std::wstring& folderPath, FILETIME& outStamp,
                            bool recursive = false);
```

Implementation:

```cpp
bool FileService::ReadFolderStamp(const std::wstring& folderPath, FILETIME& outStamp,
                                  bool recursive) {
    outStamp = {};
    if (folderPath.empty() || IsVirtualSentinel(folderPath)) return false;

    WIN32_FILE_ATTRIBUTE_DATA fad = {};
    if (!GetFileAttributesExW(folderPath.c_str(), GetFileExInfoStandard, &fad))
        return false;
    if (!(fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return false;

    outStamp = fad.ftLastWriteTime;
    if (!recursive) return true;

    // Directory mtimes catch add, remove, and rename anywhere in the tree.
    // In-place file edits do not move a directory mtime, same limitation as
    // the existing single-folder stamp.
    std::vector<DirJob> pending;
    pending.push_back({ folderPath, L"", 0 });

    while (!pending.empty()) {
        DirJob job = std::move(pending.back());
        pending.pop_back();

        std::wstring prefix = job.path;
        if (!prefix.empty() && prefix.back() != L'\\') prefix += L'\\';

        WIN32_FIND_DATAW fd = {};
        HANDLE hFind = FindFirstFileW((prefix + L'*').c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;

        do {
            if (fd.cFileName[0] == L'.') continue;
            if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;

            if (CompareFileTime(&fd.ftLastWriteTime, &outStamp) > 0)
                outStamp = fd.ftLastWriteTime;

            if (job.depth < kMaxSubfolderDepth)
                pending.push_back({ prefix + fd.cFileName, L"", job.depth + 1 });
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    return true;
}
```

The depth cap, hidden/system skip, and reparse skip must stay identical to `CollectDirectory`. A change at the deepest scanned level must invalidate the cache.

**Cost note:** this walks the directory tree once before the scan. `StartPrewarmIfFolderChanged` and the prewarm worker each call it, so a recursive prewarm does two directory walks plus the scan. Measure on a large folder. If it is slow, pass the stamp from the staleness check into the prewarm work item and accept the small race, but do that only after measuring.

---

## Task 5: MainWindow wiring

**Objective:** Add the toggle and pass the setting into the scan and stamp.

**File:** `FanFolder/src/MainWindow.cpp`

**Step 1: Menu ID**

At the enum around line 773, add after `ID_INCLUDE_DIRS`:

```cpp
ID_INCLUDE_SUBFOLDER_FILES,
```

**Step 2: Menu item**

After the `IncludeDirectories` item around line 872:

```cpp
AppendMenuW(hMenu,
    MF_STRING | (isRecentMode ? MF_GRAYED : 0) |
    (_config.includeSubfolderFiles ? MF_CHECKED : 0),
    ID_INCLUDE_SUBFOLDER_FILES, IncludeSubfolderFilesLabel());
```

A virtual folder has no filesystem tree, so the item is useless there and should be grayed out.

**Step 3: Handler**

Next to `ID_INCLUDE_DIRS` around line 909:

```cpp
case ID_INCLUDE_SUBFOLDER_FILES:
    _config.includeSubfolderFiles = !_config.includeSubfolderFiles;
    break;
```

`changed` stays true, so the existing block saves the config and forces a prewarm.

**Step 4: Stamp calls**

`StartPrewarmIfFolderChanged` around line 523:

```cpp
const bool stampValid = FileService::ReadFolderStamp(
    _config.folderPath, now, _config.includeSubfolderFiles);
```

`StartPrewarm`'s worker around line 631:

```cpp
const bool stampValid = FileService::ReadFolderStamp(
    pw->cfg.folderPath, stamp, pw->cfg.includeSubfolderFiles);
```

**Step 5: Scan call**

Around line 633:

```cpp
auto items = FileService::ScanFolder(pw->cfg.folderPath, pw->cfg.maxItems,
                                     pw->cfg.includeDirs, pw->cfg.filterRegex,
                                     pw->cfg.sortMode, /*resolveLnk=*/true,
                                     pw->cfg.includeSubfolderFiles);
```

---

## Task 6: FanWindow wiring and labels

**Objective:** Use the setting in both fan-side scans and make nested labels readable.

**File:** `FanFolder/src/FanWindow.cpp`

**Step 1: Filter full scan** around line 441:

```cpp
std::vector<FileItem> fullScan = FileService::ScanFolder(
    _config.folderPath, 10000, _config.includeDirs,
    _config.filterRegex, _config.sortMode, /*resolveLnk=*/true,
    _config.includeSubfolderFiles);
```

**Step 2: Drop rescan** around line 1566:

```cpp
_items = FileService::ScanFolder(_config.folderPath, _config.maxItems,
                                 _config.includeDirs, _config.filterRegex,
                                 _config.sortMode, /*resolveLnk=*/true,
                                 _config.includeSubfolderFiles);
```

**Step 3: `ItemLabel`** around line 1247. Refactor the three truncation branches into one and prepend the relative folder:

```cpp
std::wstring FanWindow::ItemLabel(int idx) const {
    if (idx < 0 || idx >= (int)_items.size()) return {};
    const auto& item = _items[idx];
    if (_isPlaceholder) return item.name;
    if (item.isDirectory) return item.name;

    std::wstring displayName = item.name;
    if (!item.targetPath.empty()) {
        auto dot = displayName.rfind(L'.');
        if (dot != std::wstring::npos) {
            std::wstring ext = displayName.substr(dot);
            for (auto& c : ext) c = (wchar_t)towlower(c);
            if (ext == L".lnk") displayName = displayName.substr(0, dot);
        }
    }

    if (!_config.showExtensions) {
        auto dot = displayName.rfind(L'.');
        if (dot != std::wstring::npos && dot != 0)
            displayName = displayName.substr(0, dot);
    }

    return FitItemLabel(displayName, item.relativeDir);
}
```

Add this helper above `ItemLabel` in the same file:

```cpp
namespace {
// Keeps the file name visible when a nested item's relative folder makes the
// label too long. Root items keep the old head-truncation behavior.
std::wstring FitItemLabel(const std::wstring& name, const std::wstring& relativeDir) {
    constexpr size_t kMaxLabel = 42;
    if (relativeDir.empty()) {
        if (name.size() > kMaxLabel) return name.substr(0, 40) + L"\u2026";
        return name;
    }
    if (name.size() >= kMaxLabel) return name.substr(0, 40) + L"\u2026";

    const size_t prefixBudget = kMaxLabel - name.size() - 1;  // 1 for the backslash
    std::wstring prefix = relativeDir;
    if (prefix.size() > prefixBudget) {
        if (prefixBudget <= 1) prefix.clear();
        else prefix = L"\u2026" + prefix.substr(prefix.size() - (prefixBudget - 1));
    }
    return prefix.empty() ? name : prefix + L"\\" + name;
}
} // namespace
```

Root items keep the old 40-character plus ellipsis truncation. Nested items shrink the folder prefix first, so the file name stays readable.

**Note:** label width feeds `CalculateLayout`, so the fan widens for longer nested labels. Do not change layout constants to compensate. Test with a deep path and a long file name.

---

## Task 7: Locale label

**Objective:** Add the menu label without touching the 29 positional `Strings` tables.

**Files:**
- `FanFolder/src/Localization.h:53-57`
- `FanFolder/src/Localization.cpp:1422-1455`

**Step 1: Declaration**

```cpp
// Label for the tray-menu toggle that includes files from subfolders.
// Kept separate from the positional Strings tables, following the
// EmptyFolderLabel pattern, so the 29 locale initializers stay untouched.
const wchar_t* IncludeSubfolderFilesLabel();
```

**Step 2: Implementation**

Add after `EmptyFolderLabel()`:

```cpp
const wchar_t* IncludeSubfolderFilesLabel() {
    const LANGID lang = PRIMARYLANGID(GetUserDefaultUILanguage());
    switch (lang) {
    case 0x06: return L"Medtag filer i undermapper";                 // da
    case 0x1D: return L"Inkludera filer i undermappar";              // sv
    case 0x14: return L"Inkluder filer i undermapper";               // no
    case 0x07: return L"Dateien in Unterordnern einbeziehen";        // de
    case 0x13: return L"Bestanden in submappen meenemen";            // nl
    case 0x15: return L"Uwzgl\u0119dnij pliki w podfolderach";       // pl
    case 0x01: return L"\u062a\u0636\u0645\u064a\u0646 \u0627\u0644\u0645\u0644\u0641\u0627\u062a \u0641\u064a \u0627\u0644\u0645\u062c\u0644\u062f\u0627\u062a \u0627\u0644\u0641\u0631\u0639\u064a\u0629"; // ar
    case 0x04: return L"\u5305\u542b\u5b50\u6587\u4ef6\u5939\u4e2d\u7684\u6587\u4ef6"; // zh
    case 0x11: return L"\u30b5\u30d6\u30d5\u30a9\u30eb\u30c0\u30fc\u5185\u306e\u30d5\u30a1\u30a4\u30eb\u3092\u542b\u3081\u308b"; // ja
    case 0x12: return L"\ud558\uc704 \ud3f4\ub354\uc758 \ud30c\uc77c \ud3ec\ud568"; // ko
    case 0x41: return L"Jumuisha faili katika folda ndogo";          // sw
    case 0x0C: return L"Inclure les fichiers des sous-dossiers";     // fr
    case 0x10: return L"Includi i file nelle sottocartelle";         // it
    case 0x0A: return L"Incluir archivos de las subcarpetas";        // es
    case 0x16: return L"Incluir ficheiros das subpastas";            // pt
    case 0x19: return L"\u0412\u043a\u043b\u044e\u0447\u0430\u0442\u044c \u0444\u0430\u0439\u043b\u044b \u0438\u0437 \u043f\u043e\u0434\u043f\u0430\u043f\u043e\u043a"; // ru
    case 0x39: return L"\u0909\u092a\u092b\u093c\u094b\u0932\u094d\u0921\u0930\u094b\u0902 \u0915\u0940 \u092b\u093c\u093e\u0907\u0932\u0947\u0902 \u0936\u093e\u092e\u093f\u0932 \u0915\u0930\u0947\u0902"; // hi
    case 0x1F: return L"Alt klas\u00f6rlerdeki dosyalar\u0131 dahil et"; // tr
    case 0x0D: return L"\u05db\u05dc\u05d5\u05dc \u05e7\u05d1\u05e6\u05d9\u05dd \u05d1\u05ea\u05d9\u05e7\u05d9\u05d5\u05ea \u05de\u05e9\u05e0\u05d4"; // he
    case 0x05: return L"Zahrnout soubory v podslo\u017ek\u00e1ch";   // cs
    case 0x0B: return L"Sis\u00e4llyt\u00e4 tiedostot alikansioista"; // fi
    case 0x0E: return L"F\u00e1jlok bevon\u00e1sa az almapp\u00e1kb\u00f3l"; // hu
    case 0x08: return L"\u03a3\u03c5\u03bc\u03c0\u03b5\u03c1\u03af\u03bb\u03b7\u03c8\u03b7 \u03b1\u03c1\u03c7\u03b5\u03af\u03c9\u03bd \u03b1\u03c0\u03cc \u03c5\u03c0\u03bf\u03c6\u03b1\u03ba\u03ad\u03bb\u03bf\u03c5\u03c2"; // el
    case 0x2A: return L"Bao g\u1ed3m t\u1ec7p trong th\u01b0 m\u1ee5c con"; // vi
    case 0x21: return L"Sertakan file di subfolder";                 // id
    case 0x22: return L"\u0412\u043a\u043b\u044e\u0447\u0430\u0442\u0438 \u0444\u0430\u0439\u043b\u0438 \u0437 \u043f\u0456\u0434\u043f\u0430\u043f\u043e\u043a"; // uk
    case 0x18: return L"Include fi\u0219ierele din subfoldere";      // ro
    case 0x1E: return L"\u0e23\u0e27\u0e21\u0e44\u0e1f\u0e25\u0e4c\u0e43\u0e19\u0e42\u0e1f\u0e25\u0e40\u0e14\u0e2d\u0e23\u0e4c\u0e22\u0e48\u0e2d\u0e22"; // th
    default:   return L"Include files in subfolders";                // en
    }
}
```

Keep the file ASCII-only by using `\uXXXX` escapes, as the existing tables do. Have a native speaker review the 28 non-English strings before a Store release. If that review is not available, ship English as the fallback for the unreviewed locales and keep Danish, Swedish, Norwegian, German, Dutch, French, Spanish, and Italian.

**Verify:** switch Windows display language or temporarily force a locale in the function, open the tray menu, and confirm the label renders and the item is clickable.

---

## Task 8: README

**File:** `/workspace/win-dir-fan/README.md`

**Step 1: Feature list** around line 23. Add:

```markdown
- Optionally include files from subfolders
```

**Step 2: Settings section** after `IncludeDirectories` around line 173:

```markdown
### IncludeSubfolderFiles

**Type:** `REG_DWORD` | **Default:** `0`

Whether files inside subfolders of the selected folder also appear in the fan.
`1` = include, `0` = only files directly in the selected folder.

This does not add subfolders as items. Use `IncludeDirectories` for that.
```

**Step 3: PowerShell example**

```powershell
Set-ItemProperty -Path "HKCU:\SOFTWARE\FanFolder" -Name "IncludeSubfolderFiles" -Value 1
```

Optional follow-up: add the same bullet to the homepage repo (`olebhartvigsen/FanFolder`) feature list. Do that in a separate change so this repo stays focused.

---

## Task 9: Build and runtime verification

**Objective:** Prove the change builds on both architectures and behaves correctly on the user's Windows install.

**Step 1: Ask for permission**

`AGENTS.md` forbids commit and push without explicit permission. Ask before creating the branch or pushing.

**Step 2: CI build**

```bash
git switch -c feature/include-subfolder-files
git add FanFolder/src/Config.h FanFolder/src/Config.cpp FanFolder/src/FileService.h FanFolder/src/FileService.cpp FanFolder/src/MainWindow.cpp FanFolder/src/FanWindow.cpp FanFolder/src/Localization.h FanFolder/src/Localization.cpp README.md
git commit -m "feat: include files from subfolders"
git push -u origin feature/include-subfolder-files
gh workflow run verify.yml --ref feature/include-subfolder-files
gh run watch
```

Expected: both `x64` and `ARM64` jobs pass, and `FanFolder-verify-x64` contains `FanFolder.exe`.

**Step 3: Deploy locally**

Use the `fanfolder-update-local-exe` skill (scenario B): download the x64 artifact, back up the installed exe, copy the new one, kill and relaunch, then compare MD5.

**Step 4: Manual checklist**

| Check | Expected |
|---|---|
| Setting off, existing folder | Same items and order as the previous build |
| Setting on, folder with nested files | Nested files appear |
| Nested label | Shows `Subfolder\file` (or `Subfolder\file.ext` with extensions on) |
| Two files with the same leaf name | Two distinct labels |
| Filter by subfolder name | Nested matches appear; Enter launches the top match |
| Filter by file name | Nested match appears |
| Sort by date modified | Root and nested items sort together correctly |
| Sort by name | Stable order for duplicate leaf names |
| `IncludeDirectories` off | No root folders; nested files still present |
| Virtual sentinel selected | Menu item grayed; behavior unchanged |
| Junction loop in the tree | No hang, no duplicate items |
| Hidden subfolder | Not listed |
| Nine levels deep | Level 9 is absent, level 8 is present |
| Only empty subfolders | Empty-folder placeholder still appears |
| Drag a file onto the fan | File moves to the root folder and the list refreshes |
| Right-click a nested item | Windows context menu works; rename and delete work |
| Restart the app | Setting persists |
| `MaxItems` 15 with more files | Top 15 by the chosen sort |

**Step 5: Registry sanity**

```powershell
reg query HKCU\SOFTWARE\FanFolder /v IncludeSubfolderFiles
```

Expected: `REG_DWORD` with the value matching the last toggle.

---

## Task 10: Release chores (only when cutting a release)

- Bump `FanFolder/resources/app.rc` `FILEVERSION` and `PRODUCTVERSION`.
- The release workflow injects the MSIX version from the tag or `workflow_dispatch` input, so `AppxManifest.xml` does not need a manual version edit for CI builds.
- Confirm the 29 locale strings are in the build.
- Update the Store listing only if the feature bullet changes.

---

## Risks and tradeoffs

1. **Cache staleness for in-place edits.** A file edit changes its own mtime, not its folder's mtime, so the cached order can be stale until the folder is toggled or the app restarts. This is existing behavior at the root and now applies deeper. A later change could include file mtimes in the stamp, at the cost of a full file walk.
2. **Two directory walks per recursive prewarm.** The staleness check and the worker each walk the tree. Measure on a folder with thousands of files. If it hurts, pass the first stamp into the worker and accept the small race.
3. **The 20,000 item cap biases the result.** Once the cap is hit, only the traversed subset is sorted, so the top N by date may not be the true top N for the whole tree. The cap is a safety valve; log it and revisit if users hit it.
4. **MAX_PATH.** `FindFirstFileW` on a normal path fails past 260 characters. Deep trees make that more likely. This is an existing limitation of `ScanFolder`; a follow-up could add a `\\?\` prefix helper.
5. **Longer labels widen the fan.** The 42-character cap limits the damage, but deep relative paths still eat into the file name. Test with a realistic deep tree.
6. **Translations.** 29 strings need review. Shipping unreviewed machine translations on the Store is worse than an English fallback.
7. **Reparse points are skipped.** Files behind a junction or symlinked folder, including some cloud-backed folders, will not appear. This is deliberate to avoid cycles and duplicate trees.
8. **`FileItem` grows by one `std::wstring`.** Memory per scan grows slightly. With the 20,000 cap it stays bounded.

## Open questions for the user

1. Should nested folders themselves appear as items when `IncludeDirectories` is on? Recommendation: no, only root folders.
2. Is depth 8 enough, or should it be higher? Recommendation: 8.
3. Should nested labels include the relative path? Recommendation: yes.
4. Should all 29 locales be translated now, or should unreviewed locales fall back to English? Recommendation: add all 29, review the non-English ones before release.
5. Should the item cap be 20,000, or unlimited? Recommendation: 20,000 for v1.
6. Should the toggle be grayed out for virtual folders? Recommendation: yes.

## Execution note

This container has no MSVC, no `dotnet`, no `/mnt/c`, and `gh` is not on `PATH` (the binary sits in `/opt/data/home/.local/bin`). The build gate is GitHub Actions on `windows-2022`, and runtime behavior can only be checked on the user's Windows machine. Do not claim the feature works from a Linux-only shell. A green CI build proves compilation, not behavior.
