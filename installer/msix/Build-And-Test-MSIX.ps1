# Build-And-Test-MSIX.ps1
# ============================================================================
# Complete one-shot script: build FanFolder.exe → package MSIX → sign → install
#
# Usage (on Windows, from the win-dir-fan repo root):
#   .\installer\msix\Build-And-Test-MSIX.ps1
#   .\installer\msix\Build-And-Test-MSIX.ps1 -Arch x64
#   .\installer\msix\Build-And-Test-MSIX.ps1 -Arch x64 -UninstallFirst
#
# Prerequisites (the script checks for these):
#   - Visual Studio 2022 Build Tools (CMake + MSVC)
#   - Windows 10 SDK 10.0.26100.0 (MakeAppx, SignTool)
#   - .NET SDK 9+ (for WiX — only needed for MSI, not MSIX)
# ============================================================================

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("x64", "arm64")]
    [string]$Arch = "x64",

    [Parameter(Mandatory=$false)]
    [string]$Version = "1.3.1.0",

    [Parameter(Mandatory=$false)]
    [switch]$UninstallFirst,

    [Parameter(Mandatory=$false)]
    [switch]$SkipBuild,      # Skip CMake build if FanFolder.exe already exists

    [Parameter(Mandatory=$false)]
    [switch]$KeepStaging     # Don't clean staging dir (for debugging)
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$msixDir   = $scriptDir
$root      = Split-Path -Parent (Split-Path -Parent $scriptDir)  # win-dir-fan root

function Write-Section($msg) {
    Write-Host ""
    Write-Host "=== $msg ===" -ForegroundColor Cyan
}

function Write-OK($msg)    { Write-Host "  [OK] $msg" -ForegroundColor Green }
function Write-Info($msg)  { Write-Host "  ..  $msg" -ForegroundColor Gray }
function Write-Warn($msg)  { Write-Host "  !!  $msg" -ForegroundColor Yellow }
function Write-Err($msg)   { Write-Host "  [ERROR] $msg" -ForegroundColor Red }

# ──────────────────────────────────────────────────────────────
# 0. Uninstall previous MSIX if requested
# ──────────────────────────────────────────────────────────────
if ($UninstallFirst) {
    Write-Section "Uninstalling previous FanFolder MSIX"
    $pkg = Get-AppxPackage -Name "*FanFolder*" -ErrorAction SilentlyContinue
    if ($pkg) {
        Write-Info "Found: $($pkg.PackageFullName)"
        Remove-AppxPackage -Package $pkg.PackageFullName
        Write-OK "Uninstalled"
    } else {
        Write-Info "No previous MSIX installation found"
    }
}

# ──────────────────────────────────────────────────────────────
# 1. Locate build tools
# ──────────────────────────────────────────────────────────────
Write-Section "Checking prerequisites"

# CMake — prefer VS's bundled one, fall back to PATH
$cmake = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) {
    $cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
}
if (-not (Test-Path $cmake)) {
    $cmake = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source
}
if (-not $cmake -or -not (Test-Path $cmake)) {
    Write-Err "CMake not found. Install Visual Studio 2022 Build Tools with CMake."
    exit 1
}
Write-OK "CMake: $cmake"

# Windows SDK — MakeAppx + SignTool
$sdkBinRoot = "C:\Program Files (x86)\Windows Kits\10\bin"
$sdkVersion = (Get-ChildItem $sdkBinRoot -Directory | Where-Object Name -like "10.0.*" | Sort-Object Name -Descending | Select-Object -First).Name
if (-not $sdkVersion) {
    Write-Err "Windows 10 SDK not found at $sdkBinRoot"
    exit 1
}
$sdkArch = if ($Arch -eq "arm64") { "arm64" } else { "x64" }
$makeAppx = Join-Path $sdkBinRoot "$sdkVersion\$sdkArch\makeappx.exe"
$signTool = Join-Path $sdkBinRoot "$sdkVersion\$sdkArch\signtool.exe"

# Fall back to x86 SDK tools if arch-specific not found
if (-not (Test-Path $makeAppx)) {
    $makeAppx = Join-Path $sdkBinRoot "$sdkVersion\x86\makeappx.exe"
    $signTool = Join-Path $sdkBinRoot "$sdkVersion\x86\signtool.exe"
}
if (-not (Test-Path $makeAppx)) {
    Write-Err "makeappx.exe not found in SDK $sdkVersion"
    exit 1
}
Write-OK "SDK: $sdkVersion ($sdkArch)"
Write-OK "MakeAppx: $makeAppx"

# Icon file for asset generation
$iconPath = Join-Path $root "FanFolder\resources\app.ico"
if (-not (Test-Path $iconPath)) {
    Write-Err "app.ico not found: $iconPath"
    exit 1
}
Write-OK "Icon: $iconPath"

# ──────────────────────────────────────────────────────────────
# 2. Build FanFolder.exe with CMake
# ──────────────────────────────────────────────────────────────
$exePath = Join-Path $root "FanFolder\build-$Arch\Release\FanFolder.exe"

if ($SkipBuild -and (Test-Path $exePath)) {
    Write-Section "Skipping build (FanFolder.exe already exists)"
    Write-OK $exePath
} else {
    Write-Section "Building FanFolder.exe ($Arch Release)"

    $buildDir = Join-Path $root "FanFolder\build-$Arch"
    $cmakeArch = if ($Arch -eq "arm64") { "ARM64" } else { "x64" }

    Write-Info "Configure: cmake -B $buildDir -G 'Visual Studio 17 2022' -A $cmakeArch"
    & $cmake -B $buildDir -G "Visual Studio 17 2022" -A $cmakeArch -S (Join-Path $root "FanFolder")
    if ($LASTEXITCODE -ne 0) { Write-Err "CMake configure failed"; exit 1 }
    Write-OK "Configure complete"

    Write-Info "Build: cmake --build $buildDir --config Release"
    & $cmake --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) { Write-Err "CMake build failed"; exit 1 }
    Write-OK "Build complete: $exePath"
}

if (-not (Test-Path $exePath)) {
    Write-Err "FanFolder.exe not found after build: $exePath"
    exit 1
}

# ──────────────────────────────────────────────────────────────
# 3. Prepare MSIX staging directory
# ──────────────────────────────────────────────────────────────
Write-Section "Preparing MSIX package"

$staging = Join-Path $msixDir "staging-$Arch"
$output  = Join-Path $msixDir "output"
$msixFile = Join-Path $output "FanFolder-$Arch.msix"

if (-not $KeepStaging -and (Test-Path $staging)) { Remove-Item $staging -Recurse -Force }
if (Test-Path $msixFile) { Remove-Item $msixFile -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null
New-Item -ItemType Directory -Force -Path $output | Out-Null

# Copy AppxManifest.xml
Copy-Item (Join-Path $msixDir "AppxManifest.xml") $staging -Force
Write-OK "AppxManifest.xml copied"

# Update version in manifest
$manifestPath = Join-Path $staging "AppxManifest.xml"
[xml]$manifest = Get-Content $manifestPath
$manifest.Package.Identity.Version = $Version
$manifest.Save($manifestPath)
Write-OK "Version set to $Version"

# Copy FanFolder.exe
Copy-Item $exePath (Join-Path $staging "FanFolder.exe") -Force
Write-OK "FanFolder.exe copied"

# ──────────────────────────────────────────────────────────────
# 4. Generate Store logo PNGs from app.ico
# ──────────────────────────────────────────────────────────────
Write-Section "Generating MSIX asset PNGs from app.ico"

$assetsDir = Join-Path $staging "assets"
New-Item -ItemType Directory -Force -Path $assetsDir | Out-Null

Add-Type -AssemblyName System.Drawing

$requiredAssets = [ordered]@{
    "StoreLogo.png"     = @{W=50;  H=50}
    "Logo44.png"        = @{W=44;  H=44}
    "Logo71.png"        = @{W=71;  H=71}
    "Logo150.png"       = @{W=150; H=150}
    "Logo310.png"       = @{W=310; H=310}
    "Wide310x150.png"   = @{W=310; H=150}
    "SplashScreen.png"  = @{W=620; H=300}
}

# Check if pre-made PNG assets exist in msix\assets\
$sourceAssetsDir = Join-Path $msixDir "assets"

$ico = [System.Drawing.Icon]::new($iconPath)
foreach ($name in $requiredAssets.Keys) {
    $dest = Join-Path $assetsDir $name
    $src  = Join-Path $sourceAssetsDir $name

    if (Test-Path $src) {
        Copy-Item $src $dest -Force
        Write-Info "$name (from pre-made)"
    } else {
        $dim = $requiredAssets[$name]
        $w = $dim.W; $h = $dim.H

        $bmp = New-Object System.Drawing.Bitmap $w, $h
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.Clear([System.Drawing.Color]::Transparent)
        $g.DrawImage($ico.ToBitmap(), 0, 0, $w, $h)
        $g.Dispose()
        $bmp.Save($dest, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Info "$name ($w x $h) generated"
    }
}
$ico.Dispose()
Write-OK "All assets ready"

# ──────────────────────────────────────────────────────────────
# 5. Pack MSIX
# ──────────────────────────────────────────────────────────────
Write-Section "Packing MSIX"

Write-Info "makeappx pack -d $staging -p $msixFile"
& $makeAppx pack -d $staging -p $msixFile -v
if ($LASTEXITCODE -ne 0) { Write-Err "MakeAppx pack failed"; exit 1 }

$msixSize = [math]::Round((Get-Item $msixFile).Length / 1KB)
Write-OK "MSIX: $msixFile ($msixSize KB)"

# ──────────────────────────────────────────────────────────────
# 6. Self-sign for local testing
# ──────────────────────────────────────────────────────────────
Write-Section "Self-signing MSIX (for local test only)"

$certSubject = "CN=FanFolder Test Signing"
$certPath = Join-Path $msixDir "FanFolderTestCert.pfx"
$certPass = "FanFolderTest123!"
$secPass  = ConvertTo-SecureString $certPass -Force -AsPlainText

# Create test cert if it doesn't exist
if (-not (Test-Path $certPath)) {
    Write-Info "Creating self-signed code signing certificate..."
    $cert = New-SelfSignedCertificate -Type CodeSigningCert `
        -Subject $certSubject `
        -KeyUsage DigitalSignature `
        -FriendlyName "FanFolder Test Signing" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -KeyAlgorithm RSA -KeyLength 2048 `
        -HashAlgorithm SHA256 `
        -NotAfter (Get-Date).AddYears(1)

    Export-PfxCertificate -Cert $cert -FilePath $certPath -Password $secPass
    Write-OK "Cert created: $certPath"
} else {
    Write-OK "Reusing existing cert: $certPath"
}

# Sign the MSIX
Write-Info "signtool sign -fd SHA256 $msixFile"
& $signTool sign -fd SHA256 -f $certPath -p $certPass $msixFile
if ($LASTEXITCODE -ne 0) { Write-Err "SignTool failed"; exit 1 }
Write-OK "Signed with SHA256"

# Install cert to Trusted People (required for MSIX installation)
Write-Info "Installing cert to Trusted People..."
$cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 $certPath, $secPass
$store = New-Object System.Security.Cryptography.X509Certificates.X509Store "TrustedPeople", "CurrentUser"
$store.Open("ReadWrite")

# Remove old cert if present
$existing = $store.Certificates | Where-Object { $_.Subject -eq $certSubject }
if ($existing) {
    $store.Remove($existing)
    Write-Info "Removed old cert"
}
$store.Add($cert)
$store.Close()
Write-OK "Cert installed to Trusted People (CurrentUser)"

# ──────────────────────────────────────────────────────────────
# 7. Install MSIX
# ──────────────────────────────────────────────────────────────
Write-Section "Installing MSIX"

# Remove old installation if present
$pkg = Get-AppxPackage -Name "*FanFolder*" -ErrorAction SilentlyContinue
if ($pkg) {
    Write-Info "Removing previous installation: $($pkg.PackageFullName)"
    Remove-AppxPackage -Package $pkg.PackageFullName
    Start-Sleep -Seconds 2
}

Write-Info "Add-AppxPackage -Path $msixFile"
Add-AppxPackage -Path $msixFile
if ($?) {
    Write-OK "Installed successfully"
} else {
    Write-Err "Installation failed"
    Write-Warn "Try enabling Developer Mode: Settings → For developers → Developer Mode"
    exit 1
}

# ──────────────────────────────────────────────────────────────
# 8. Summary
# ──────────────────────────────────────────────────────────────
Write-Section "Done — FanFolder MSIX installed"

$pkg = Get-AppxPackage -Name "*FanFolder*"
Write-Host ""
Write-Host "  Package:  $($pkg.PackageFullName)" -ForegroundColor White
Write-Host "  Version:  $($pkg.Version)" -ForegroundColor White
Write-Host "  Arch:     $($pkg.Architecture)" -ForegroundColor White
Write-Host "  Path:     $($pkg.InstallLocation)" -ForegroundColor White
Write-Host ""
Write-Host "  FanFolder should now be in your Start Menu." -ForegroundColor Green
Write-Host "  Launch it to test." -ForegroundColor Green
Write-Host ""
Write-Host "  Uninstall:  Remove-AppxPackage -Package '$($pkg.PackageFullName)'" -ForegroundColor Gray
Write-Host ""
