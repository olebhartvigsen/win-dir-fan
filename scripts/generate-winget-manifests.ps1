param(
    [string]$Version,
    [string]$Tag,
    [string]$X64Msi   = "artifacts/x64/installer/output/FanFolderSetup-x64.msi",
    [string]$Arm64Msi = "artifacts/arm64/installer/output/FanFolderSetup-arm64.msi",
    [string]$OutDir   = "winget-manifests",
    [string]$ReleaseNotesPath = "RELEASE_DRAFT.md"
)

$baseUrl = "https://github.com/olebhartvigsen/FanFolder/releases/download/$Tag"

$sha256_x64   = (Get-FileHash $X64Msi   -Algorithm SHA256).Hash
$sha256_arm64 = (Get-FileHash $Arm64Msi -Algorithm SHA256).Hash

function Get-MsiProductCode($path) {
    $installer = New-Object -ComObject WindowsInstaller.Installer
    $db     = $installer.GetType().InvokeMember("OpenDatabase", "InvokeMethod", $null, $installer, @((Resolve-Path $path).Path, 0))
    $view   = $db.GetType().InvokeMember("OpenView",      "InvokeMethod", $null, $db,        @("SELECT Value FROM Property WHERE Property='ProductCode'"))
    $view.GetType().InvokeMember("Execute", "InvokeMethod", $null, $view, $null)
    $record = $view.GetType().InvokeMember("Fetch",        "InvokeMethod", $null, $view,      $null)
    $value  = $record.GetType().InvokeMember("StringData",  "GetProperty",  $null, $record,    @(1))
    return   $value.Trim()
}

$pc_x64   = Get-MsiProductCode $X64Msi
$pc_arm64 = Get-MsiProductCode $Arm64Msi

# Extract release notes (bullet lines) from RELEASE_DRAFT.md for the locale manifest.
$releaseNotesBlock = ""
if (Test-Path $ReleaseNotesPath) {
    $bullets = Get-Content $ReleaseNotesPath | Where-Object { $_ -match '^\s*-\s+' } | ForEach-Object { "  " + $_.TrimStart() }
    if ($bullets.Count -gt 0) {
        $releaseNotesBlock = "ReleaseNotes: |-`n  Highlights in $Version`:`n" + ($bullets -join "`n") + "`n"
    }
}

New-Item -ItemType Directory -Force $OutDir | Out-Null

# ── version manifest ──────────────────────────────────────────────────────────
@"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.version.1.9.0.schema.json
PackageIdentifier: OleBhartvigsen.FanFolder
PackageVersion: $Version
DefaultLocale: en-US
ManifestType: version
ManifestVersion: 1.9.0
"@ | Set-Content "$OutDir/OleBhartvigsen.FanFolder.yaml" -Encoding UTF8

# ── installer manifest ────────────────────────────────────────────────────────
@"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.installer.1.9.0.schema.json
PackageIdentifier: OleBhartvigsen.FanFolder
PackageVersion: $Version
InstallerLocale: en-US
Platform:
  - Windows.Desktop
MinimumOSVersion: 10.0.17763.0
InstallerType: msi
InstallModes:
  - silent
  - silentWithProgress
UpgradeBehavior: install
Installers:
  - Architecture: x64
    InstallerUrl: $baseUrl/FanFolderSetup-x64.msi
    InstallerSha256: $sha256_x64
    ProductCode: '$pc_x64'
    Scope: user
    ElevationRequirement: elevationProhibited
    InstallerSwitches:
      Silent: /quiet /norestart
      SilentWithProgress: /passive /norestart
  - Architecture: arm64
    InstallerUrl: $baseUrl/FanFolderSetup-arm64.msi
    InstallerSha256: $sha256_arm64
    ProductCode: '$pc_arm64'
    Scope: user
    ElevationRequirement: elevationProhibited
    InstallerSwitches:
      Silent: /quiet /norestart
      SilentWithProgress: /passive /norestart
ManifestType: installer
ManifestVersion: 1.9.0
"@ | Set-Content "$OutDir/OleBhartvigsen.FanFolder.installer.yaml" -Encoding UTF8

# ── locale manifest ───────────────────────────────────────────────────────────
@"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.defaultLocale.1.9.0.schema.json
PackageIdentifier: OleBhartvigsen.FanFolder
PackageVersion: $Version
PackageLocale: en-US
Publisher: Ole Bhartvigsen
PublisherUrl: https://github.com/olebhartvigsen
PublisherSupportUrl: https://github.com/olebhartvigsen/FanFolder/issues
Author: Ole Bhartvigsen
PackageName: FanFolder
PackageUrl: https://github.com/olebhartvigsen/FanFolder
License: Proprietary
LicenseUrl: https://github.com/olebhartvigsen/FanFolder/blob/main/LICENSE
Copyright: Copyright (c) 2026 Ole Bulow Hartvigsen
CopyrightUrl: https://github.com/olebhartvigsen/FanFolder/blob/main/LICENSE
ShortDescription: Animated fan folder popup for the Windows taskbar.
Description: |-
  FanFolder turns any folder into an animated, arc-shaped popup on the Windows
  taskbar. Click the taskbar icon and the most recently modified items in a
  configured folder fan out in a smooth animation. Open items, use the full
  Windows shell context menu (right-click), or drag them into other applications.

  Works with local folders as well as cloud-synced folders such as OneDrive,
  Dropbox and Google Drive — always showing the latest files.

  Key features:
  - Multiple animation styles: Fan, Glide, Spring, Fade or None.
  - Sort by date modified, date created or name, with optional filename regex filter.
  - Configurable item count, folder path and display options via tray menu or registry.
  - Full Windows shell context menu and drag-and-drop support.
  - Per-user MSI install, no admin rights required.
  - Native x64 and ARM64 builds.
  - Small footprint and low memory usage.
Moniker: fanfolder
Tags:
- taskbar
- productivity
- launcher
- fan
- folder
- utility
- shell
- files
`${releaseNotesBlock}ReleaseNotesUrl: https://github.com/olebhartvigsen/FanFolder/releases/tag/v$Version
ManifestType: defaultLocale
ManifestVersion: 1.9.0
"@ | Set-Content "$OutDir/OleBhartvigsen.FanFolder.locale.en-US.yaml" -Encoding UTF8

Write-Host "Winget manifests written to $OutDir"
Write-Host "  x64   SHA256: $sha256_x64"
Write-Host "  arm64 SHA256: $sha256_arm64"
Write-Host "  x64   ProductCode: $pc_x64"
Write-Host "  arm64 ProductCode: $pc_arm64"
