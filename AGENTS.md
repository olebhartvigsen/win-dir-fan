# AGENTS.md — Instruktioner for AI-agenter i FanFolder workspace

## Workspace Oversigt

Dette repository indeholder FanFolder, en letvægts Win32/C++ taskbar-utility til Windows. Appen viser recent files som en animeret fan-menu på proceslinjen. Bygges som EXE (det traditionelle format) og MSIX (til Microsoft Store).

## Obligatoriske regler

- ✅ **Brug humanizer-skill'en** (`humanizer`) på alt genereret tekst og al tekst der gennemgås/redigeres — det er obligatorisk. Indlæs skill'en med `skill_view(name='humanizer')` og følg dens 34 mønstre før tekst leveres til brugeren eller skrives til fil. Gælder for alle sprog (engelsk, dansk, etc.)
- ❌ **Brug ikke em dashes** (—) i bruger-rettet tekst — brug komma, punktum eller parenteser i stedet
- ❌ **Commit eller push ikke** uden eksplicit tilladelse fra brugeren
- ❌ **Læs eller print ikke secrets** — lad `.env` og credential-filer ligge i fred
- ✅ **Verificer med \``git status\`/\`git branch\`** før du stoler på Workspace-snapshots

## Teknologistak

- **Sprog**: C++ (Win32 API), PowerShell (build scripts)
- **Build**: CMake, Visual Studio 2022 Build Tools, Windows SDK 10.0.26100.0
- **CI**: GitHub Actions (`.github/workflows/release.yml`), `windows-latest` runner
- **Lokationering**: `FanFolder/src/Localization.cpp` (29 sprog som `kXX` konstanter)
- **MSIX**: `installer/msix/AppxManifest.xml` + CI `makeappx` build
- **Store**: Microsoft Partner Center, MSIX-format, 29 sprog

## Vigtige konventioner

- **CI sed**: Scope til `<Identity` for Version-udskiftning — blanket `Version="..."` overskriver MinVersion
- **CI ProcessorArchitecture**: Inject per-arch via `sed` i "Copy manifest to staging dirs" step
- **AppxManifest**: Bash `cp` + `sed` til alle transformationer (undgår UTF-8 BOM → C00CEE40)
- **Partner Center identitet**: Name=`Hartvigsen.FanFolder`, Publisher=`CN=26E1ACFC-F324-4E77-8BEF-404C2340AA56`, PublisherDisplayName=`Hartvigsen`
- **MSIX CSV-import**: Brug `Title` (ikke `ProductName`), `Feature1` (ikke `ProductFeatures1`), `DesktopScreenshot1` (ikke `Screenshots1`), etc.
- **Windows min version**: MSIX kræver 2004 (build 19041), ikke 1809

## Mappestruktur

| Sti | Formål |
|---|---|
| `FanFolder/src/` | C++ kildekode, herunder `Localization.cpp` |
| `FanFolder/resources/` | Ikoner (`.ico`), app resources |
| `FanFolder/windows store/` | Store listing assets (screenshots, logos, CSV) |
| `installer/msix/` | AppxManifest.xml + MSIX build scripts |
| `installer/store/` | Store submission guides, CSV listings, capability justifications |
| `.github/workflows/` | CI pipelines (release.yml) |
