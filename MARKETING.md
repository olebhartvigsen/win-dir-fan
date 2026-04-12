# FanFolder — Marketing & Distribution Guide

FanFolder is a free, lightweight Win32 utility that brings the macOS Dock "fan" folder experience to the Windows taskbar. Below are 10 realistic, actionable initiatives to grow its audience, followed by a guide on how to get it listed in the major software directories.

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
- [r/mac](https://www.reddit.com/r/mac) — angle: "miss the Mac dock fan? Now on Windows"

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
"macOS dock for Windows" or "HiDock alternative" will find it.  
- Add FanFolder as an alternative to macOS Dock / HiDock / RocketDock  
- Free to list; just create an account and submit

---

### 5. 📦 Submit a winget package (Windows Package Manager)
`winget` is built into Windows 11 and lets users install apps with one command:
```
winget install FanFolder
```
Steps:
1. Create a release on GitHub with the `FanFolder.exe` (handled by the CI pipeline)
2. Fork the [winget-pkgs](https://github.com/microsoft/winget-pkgs) repo
3. Use [WingetCreate](https://github.com/microsoft/winget-create) to auto-generate the manifest from your GitHub release URL
4. Submit a pull request — Microsoft reviews and merges within a few days

This is **free**, requires no code signing, and gives huge discoverability on Windows 11.

---

### 6. 🟡 Show HN on Hacker News
[Hacker News Show HN](https://news.ycombinator.com/show) posts reach developers and power users worldwide.  
- Title: `Show HN: FanFolder – macOS-style animated fan folder for the Windows taskbar`  
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
- **Twitter/X**: Short thread with the GIF, tag `#Windows`, `#WindowsTips`, `#opensource`
- **LinkedIn**: Frame it as a productivity story — "I built this because I missed macOS..."
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

### Windows Package Manager (winget) — Recommended first step
**Cost:** Free | **Effort:** Low | **Reach:** All Windows 11 users

1. Ensure the GitHub Release has a direct `.exe` download URL
2. Install WingetCreate: `winget install Microsoft.WingetCreate`
3. Run: `wingetcreate new https://github.com/olebhartvigsen/win-dir-fan/releases/download/vX.X.X/FanFolder.exe`
4. Fill in the prompted fields (publisher, description, license)
5. Submit the generated YAML files as a PR to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs)

> ⚠️ winget does **not** require code signing for community packages.

---

### Microsoft Store — Longer term
**Cost:** $19 one-time developer registration | **Effort:** Medium

The Microsoft Store now accepts Win32 apps packaged as **MSIX** via the Desktop App Converter or directly with Visual Studio.

Steps:
1. Register at [partner.microsoft.com](https://partner.microsoft.com/en-us/dashboard) ($19 one-time fee)
2. Package the app as MSIX:
   - Use **MSIX Packaging Tool** (free, from the Store) to wrap `FanFolder.exe` into an MSIX
   - Or add an MSIX packaging project in Visual Studio
3. **Code signing is required** for Store submission:
   - Purchase a code signing certificate (~$70–$200/year from Sectigo, DigiCert, etc.)
   - Or apply for the [Microsoft Trusted Signing](https://learn.microsoft.com/en-us/azure/trusted-signing/) service (cheaper, $9.99/month)
4. Submit via Partner Center — review typically takes 1–3 business days

> 💡 Start with **winget** first (free, no signing required). Add the Store later if there's demand.

---

### GitHub Releases — Already set up ✅
The CI pipeline automatically attaches `FanFolder.exe` to every published GitHub Release.  
Direct download link pattern:
```
https://github.com/olebhartvigsen/win-dir-fan/releases/latest/download/FanFolder.exe
```
Use this URL in all listings and blog posts.

---

### Chocolatey (community package manager)
**Cost:** Free | **Effort:** Low

Similar to winget but older and still widely used:
1. Create an account at [chocolatey.org](https://chocolatey.org)
2. Create a `.nuspec` package definition pointing to the GitHub release
3. Submit to the [Chocolatey Community Repository](https://community.chocolatey.org/packages)

---

*Last updated: April 2026*
