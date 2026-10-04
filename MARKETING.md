# FanFolder — Marketing & Distribution Guide

FanFolder is a free, lightweight Win32 utility that adds an animated "fan" folder popup to the Windows taskbar. Below are 10 realistic, actionable initiatives to grow its audience, followed by a guide on how to get it listed in the major software directories.

---

## 10 Marketing Initiatives

### 1. 🎬 Create a 30-second GIF/video demo for the README
The single most effective thing for a GitHub utility. Record a short screen capture showing:  
- Clicking the taskbar icon  
- The fan animation opening  
- Hovering, dragging, right-clicking a file  

Add it to the top of `README.md`. Tools: [ScreenToGif](https://www.screentogif.com/) (free).

---

### 2. 🟠 Post on Reddit
Target these subreddits with a short write-up + the demo GIF:
- [r/Windows](https://www.reddit.com/r/windows) — largest Windows audience
- [r/windowsapps](https://www.reddit.com/r/windowsapps) — dedicated to Windows utilities
- [r/productivity](https://www.reddit.com/r/productivity) — focus on workflow improvement angle

**Tip:** Post on a weekday morning (US time). Lead with the GIF, not a wall of text.

---

### 3. 🚀 Submit to Product Hunt
[Product Hunt](https://www.producthunt.com) is the go-to launch platform for utilities and tools.  
- Create a free account and submit as a product  
- Add screenshots, a short tagline, and the demo GIF  
- Ask friends/colleagues to upvote on launch day  
- A good PH launch drives real GitHub stars and installs

---

### 4. 🔍 List on AlternativeTo.net
[AlternativeTo](https://alternativeto.net) is heavily indexed by Google. People searching for  
"animated taskbar launcher" or "HiDock alternative" will find it.  
- Add FanFolder as an alternative to HiDock / RocketDock and similar taskbar launchers  
- Free to list; just create an account and submit

---

### 5. 📦 Submit a winget package (Windows Package Manager)
`winget` is built into Windows 11 and lets users install apps with one command:
```
winget install OleBhartvigsen.FanFolder
```

The package identifier is `OleBhartvigsen.FanFolder`. The bare name `FanFolder` is not a valid package,
so `winget install FanFolder` fails. Manifests live at `manifests/o/OleBhartvigsen/FanFolder/` in
winget-pkgs.

Status: **live**. Versions 1.2.2, 1.3.0 and 1.4.3 are published. `scripts/generate-winget-manifests.ps1`
generates all three manifest files during CI and attaches them to every release, so a new version means
tagging a release and submitting a PR with the generated YAML.

Steps for the next version:
1. Publish a GitHub release (CI builds the MSIs and generates the manifests from the fresh artifacts)
2. Download the three generated `.yaml` files from the release assets and validate them
3. Open a PR against [winget-pkgs](https://github.com/microsoft/winget-pkgs)

This is **free**, requires no code signing, and gives huge discoverability on Windows 11.

---

### 6. 🟡 Show HN on Hacker News
[Hacker News Show HN](https://news.ycombinator.com/show) posts reach developers and power users worldwide.  
- Title: `Show HN: FanFolder – animated fan folder popup for the Windows taskbar`  
- Include a link to the GitHub repo  
- Be ready to respond to comments quickly — engagement in the first hour matters most

---

### 7. 📝 Reach out to Windows utility blogs
These sites actively cover small Windows utilities and are often found at the top of Google results:
- **Ghacks.net** — email the editor with a short pitch + GIF
- **Windows Report** (windowsreport.com) — has a "tools" section
- **AddictiveTips** (addictivetips.com) — frequently covers free Windows tools
- **NirBlog** (nirsoft.net/blog) — NirSoft is a respected voice in the Windows utilities space

A single article on any of these can drive thousands of downloads.

---

### 8. 📣 Post on social media with the demo video
- **Twitter/X**: Short thread with the GIF, tag `#Windows`, `#WindowsTips`, `#taskbar`, `#productivity` (do **not** tag `#opensource`: FanFolder is proprietary with source withheld)
- **LinkedIn**: Frame it as a productivity story — "I built this small utility because Windows needed it..."
- **TikTok / YouTube Shorts**: 30-second "did you know Windows can do this?" style video — these go viral organically in the tech niche

---

### 9. 🗂️ Submit to software aggregator sites
These directories are crawled by Google and drive long-tail organic traffic:
- **Softpedia** (softpedia.com/submit) — large user base, free submission
- **FileHippo** (filehippo.com) — popular download site
- **SourceForge** (sourceforge.net) — create a project mirror
- **FossHub** (fosshub.com) — clean, trusted, targets open-source tools

All are free and require just a description, screenshots, and the installer/exe.

---

### 10. 🌐 Build a minimal landing page
A simple one-page site (GitHub Pages is free) beats a GitHub README for general audiences.  
Include: headline, demo GIF, one-click download button, screenshots.  
Domain cost: ~$10/year (e.g. `fanfolder.app`).  
This gives a clean URL to share everywhere and improves Google discoverability.

---

## Distribution & Registration Guide

### Windows Package Manager (winget) — Live since v1.2.2
**Cost:** Free | **Effort:** Low | **Reach:** All Windows 11 users

Package identifier: `OleBhartvigsen.FanFolder`

```
winget install OleBhartvigsen.FanFolder
```

You do not need WingetCreate by hand. `scripts/generate-winget-manifests.ps1` runs in CI on every
release and produces the three manifest files (`OleBhartvigsen.FanFolder.yaml`,
`.installer.yaml`, `.locale.en-US.yaml`) from the MSIs it just built, so the SHA256 and ProductCode
cannot drift from the shipped files. They are attached to the release as assets.

To publish a new version:
1. Merge to `master`, then create and publish the GitHub release. The `release: published` event fires
   the workflow.
2. Download the three generated `.yaml` files from the release assets.
3. Validate: version matches the tag, 64-character SHA256s, real ProductCodes, `Scope: user`, CRLF
   endings, no unresolved `${...}` template variables.
4. Open a PR against [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) at
   `manifests/o/OleBhartvigsen/FanFolder/<version>/`.

`workflow_dispatch` on the same workflow only produces a `dev-<sha>` prerelease. That is a compile
gate, not a submittable version.

> winget does **not** require code signing for community packages. The MSI is per-user, so the
> manifests use `Scope: user` and no elevation is requested.

---

### Microsoft Store — Live since v1.2.x
**Cost:** $19 one-time developer registration | **Effort:** Medium

Listing: https://apps.microsoft.com/detail/9P3VT115SBM1 (product ID `00765a5e-5d0b-4f78-af6f-bb51b3b0379b`,
MSIX x64 + ARM64, 29 languages). The CI `build-msix` job packs both packages and attaches them to the
release.

To ship a Store update:
1. Bump the MSIX version in CI (`MSIX_VERSION` follows the release tag) and publish the release.
2. Download `FanFolder-x64.msix` and `FanFolder-arm64.msix` from the release artifacts.
3. In Partner Center, create a submission and upload **both** MSIX files. Microsoft signs them. Do not
   upload the MSI here; the Store takes MSIX only.
4. Update the listing fields. The import-ready CSV is
   `FanFolder/windows store/Partner Center import - English/FanFolderListing.csv`, which carries the
   per-language columns and the `SearchTerm1..7` keyword rows.
5. Submit for review. Typically 1 to 3 business days.

> The Store listing and the winget `Tags` should say the same things. Both are search-driven surfaces
> and they drift independently if only one gets updated.

---

### GitHub Releases — Already set up ✅
The CI pipeline publishes every release into the distribution repo `olebhartvigsen/FanFolder` and
attaches the full asset set: `FanFolderSetup.exe` (bundle), `FanFolderSetup-x64.msi`,
`FanFolderSetup-arm64.msi`, `FanFolder-x64.exe`, `FanFolder-arm64.exe`, `FanFolder-x64.msix`,
`FanFolder-arm64.msix`, plus the three winget manifests.

Single installer link that works for sharing:
```
https://github.com/olebhartvigsen/FanFolder/releases/latest/download/FanFolderSetup.exe
```

There is no asset called `FanFolder.exe`, so `releases/latest/download/FanFolder.exe` returns 404. The
releases are per-architecture, and the `x64` and `arm64` names are the only way to get a direct binary
for a given machine. If in doubt, link the homepage, which routes the visitor to the right one.

---

### Chocolatey (community package manager)
**Cost:** Free | **Effort:** Low

Similar to winget but older and still widely used:
1. Create an account at [chocolatey.org](https://chocolatey.org)
2. Create a `.nuspec` package definition pointing to the GitHub release
3. Submit to the [Chocolatey Community Repository](https://community.chocolatey.org/packages)

---

*Last updated: April 2026*
