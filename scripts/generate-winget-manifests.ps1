param(
    [string]$Version,
    [string]$Tag,
    [string]$X64Msi   = "artifacts/x64/installer/output/FanFolderSetup-x64.msi",
    [string]$Arm64Msi = "artifacts/arm64/installer/output/FanFolderSetup-arm64.msi",
    [string]$OutDir   = "winget-manifests"
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
    return   $record.GetType().InvokeMember("StringData",  "GetProperty",  $null, $record,    @(1))
}

$pc_x64   = Get-MsiProductCode $X64Msi
$pc_arm64 = Get-MsiProductCode $Arm64Msi

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
    InstallerSwitches:
      Silent: /quiet /norestart
      SilentWithProgress: /passive /norestart
  - Architecture: arm64
    InstallerUrl: $baseUrl/FanFolderSetup-arm64.msi
    InstallerSha256: $sha256_arm64
    ProductCode: '$pc_arm64'
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
PackageName: FanFolder
PackageUrl: https://github.com/olebhartvigsen/FanFolder
License: MIT
LicenseUrl: https://github.com/olebhartvigsen/win-dir-fan/blob/master/LICENSE
ShortDescription: macOS-style animated fan folder for the Windows taskbar
Description: >-
  FanFolder replicates the macOS Dock "Fan" folder experience on the Windows
  taskbar. Clicking the taskbar icon reveals an animated, arc-shaped popup
  showing the most recently modified items in a configured folder. Items can
  be opened, right-clicked (full shell context menu), or dragged to other
  applications. Supports 16 languages and is fully configurable via the
  Windows registry.
Tags:
  - taskbar
  - productivity
  - launcher
  - dock
  - macos
  - fan
  - folder
  - utility
ManifestType: defaultLocale
ManifestVersion: 1.9.0
"@ | Set-Content "$OutDir/OleBhartvigsen.FanFolder.locale.en-US.yaml" -Encoding UTF8

Write-Host "Winget manifests written to $OutDir"
Write-Host "  x64   SHA256: $sha256_x64"
Write-Host "  arm64 SHA256: $sha256_arm64"
Write-Host "  x64   ProductCode: $pc_x64"
Write-Host "  arm64 ProductCode: $pc_arm64"
