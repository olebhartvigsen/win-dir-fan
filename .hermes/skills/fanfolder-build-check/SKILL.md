---
name: fanfolder-build-check
description: Use when changing FanFolder C++ on a non-Windows host and you need a real compile check before pushing.
version: 1.0.0
metadata:
  hermes:
    tags: [fanfolder, cpp, mingw, verification]
---

# Compile-checking FanFolder without Windows

The repo builds with MSVC on `windows-2022` only. On the Linux/macOS agent host there is
no MSVC, so verify with mingw-w64 g++ before claiming a change works.

## CI and release policy for this repo

- Pushing to `master` runs `.github/workflows/verify.yml` (x64 and ARM64 build,
  plus the portable match tests on x64). CI runs are pre-approved: do not ask
  before starting one.
- A release is a separate decision. `release.yml` publishes artifacts to the
  distribution repo and pushes winget manifests back to `master`, so it needs
  an explicit instruction and approval from the user every time.
- `workflow_dispatch` on `release.yml` still creates a dev prerelease. Treat it
  as a release, not as a build check.

## Get a compiler without root

```bash
mkdir -p ~/.local/mingw/deb && cd ~/.local/mingw/deb
apt-get download binutils-common binutils-mingw-w64-x86-64 gcc-mingw-w64-base \
  gcc-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix-runtime \
  mingw-w64-common mingw-w64-x86-64-dev g++-mingw-w64-x86-64-posix
for f in *.deb; do dpkg-deb -x "$f" ~/.local/mingw/root; done
export PATH=$HOME/.local/mingw/root/usr/bin:$PATH
```

The binaries are suffixed `-posix` (`x86_64-w64-mingw32-g++-posix`).

## Stub lunasvg

`FileService.cpp` includes `<lunasvg.h>`, fetched by CMake. Create a stub header in
`/tmp/fancheck/stub/lunasvg.h` declaring only what the code uses:
`Document::loadFromData(const std::string&)`, `loadFromData(const char*, size_t)`,
`width()`, `height()`, `renderToBitmap(int,int)`, and `Bitmap::isNull()`, `data()`, `stride()`.

## Run the check

```bash
cd FanFolder
for f in src/*.cpp; do
  x86_64-w64-mingw32-g++-posix -std=c++20 -fsyntax-only -DUNICODE -D_UNICODE \
    -I src -I resources -I /tmp/fancheck/stub "$f" 2>&1 | grep -c "error:"
done
```

Every file must report 0. `-fsyntax-only` still does full type checking, so it catches
wrong COM signatures, deleted-copy mistakes and most template errors.

## Full build, not just the syntax pass

`-fsyntax-only` proves types, not that the app links. A real build is possible
with the same extracted toolchain plus a CMake toolchain file.

```bash
# windres shells out to `gcc` without the -posix suffix, which is not in the
# Debian package. Symlink the three names it looks for.
mkdir -p ~/.local/mingw/bin
for t in gcc g++ cpp; do
  ln -sf ../root/usr/bin/x86_64-w64-mingw32-${t}-posix \
         ~/.local/mingw/bin/x86_64-w64-mingw32-${t}
done
export PATH=$HOME/.local/mingw/bin:$HOME/.local/mingw/root/usr/bin:$PATH

cmake -B /tmp/fanbuild -S FanFolder \
  -DCMAKE_TOOLCHAIN_FILE=/tmp/mingw-toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release -DFANFOLDER_BUILD_TESTS=ON
cmake --build /tmp/fanbuild -j4
```

The toolchain file needs `CMAKE_SYSTEM_NAME Windows`, the three `-posix`
compilers, `CMAKE_RC_COMPILER=x86_64-w64-mingw32-windres`, and
`CMAKE_FIND_ROOT_PATH=$HOME/.local/mingw/root/usr/x86_64-w64-mingw32` with
library/include modes set to ONLY.

`FetchContent` clones and builds lunasvg, so this needs network once. The
output is `Release/FanFolder.exe`. It is a real PE binary, but it is not the
shipped build: MSVC on `windows-2022` remains the release gate.

## Running the tests

`tests/MatchScoreTests.cpp` deliberately avoids Win32 so it runs anywhere:

```bash
g++ -std=c++20 -Wall -Wextra -o /tmp/matchtests \
  FanFolder/tests/MatchScoreTests.cpp FanFolder/src/MatchScore.cpp && /tmp/matchtests
```

The `.exe` built by CMake cannot run on Linux, so execute the native build
above rather than `ctest`.

## Icons and the resource script

`resources/app.rc` only compiles in the CMake build, not in the `-fsyntax-only`
pass. Any new `ICON` line must therefore be checked with the full build; a typo
in a filename fails there with `file not found`.

`scripts/make-scan-icons.py` derives the animated scanning frames
(`app-scan-*.ico`, `app-open-scan-*.ico`) from `app.ico` and `app-open.ico` with
Pillow. Run it after changing either source icon and keep the ids in
`resource.h` and `app.rc` in sync.

To prove the icons really landed in the binary, read the `.rsrc` section and
count the `RT_GROUP_ICON` entries instead of trusting the build log.

## Pitfalls

- `windres` prints `app.rc:NN: unrecognized escape sequence` for the
  `\u00fcl` in the version string. That warning predates any icon work.
- `_WIN32_WINNT` redefinition warning is expected, ignore it.
- No linter or formatter runs on `.cpp` here; read the diff by hand.
- This proves the code compiles, not that it links or behaves correctly. Anything that
  changes runtime behaviour still needs a CI run or a manual test on Windows.
- MSVC allows `std::ifstream(std::wstring)`; libstdc++ does not. Use
  `std::ifstream f{std::filesystem::path(jsonPath)};` (braces, or it parses as a function
  declaration).