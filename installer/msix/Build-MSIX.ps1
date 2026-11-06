# Build-MSIX.ps1
# Builds FanFolder MSIX package from a pre-built FanFolder.exe
#
# Prerequisites:
#   - FanFolder.exe already built (run Build-Installer.ps1 first, or build manually)
#   - Windows SDK 10.0.26100 or later installed (for MakeAppx, Signtool)
#   - PowerShell 5.1+ (Windows PowerShell or pwsh)
#
# Usage:
#   .\Build-MSIX.ps1 -Arch x64
#   .\Build-MSIX.ps1 -Arch arm64
#   .\Build-MSIX.ps1 -Arch x64 -ExePath "C:\path\to\FanFolder.exe"
#
# Output:
#   installer\msix\output\FanFolder-<arch>.msix

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("x64", "arm64")]
    [string]$Arch = "x64",

    [Parameter(Mandatory=$false)]
    [string]$ExePath,

    [Parameter(Mandatory=$false)]
    [string]$Version = "1.3.1.0",

    [Parameter(Mandatory=$false)]
    [switch]$Sign  # -Sign to self-sign for local testing only
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $scriptDir  # win-dir-fan root

# ── Locate FanFolder.exe ─────────────────────────────────────
if ($ExePath) {
    $exe = $ExePath
} else {
    $exe = Join-Path $root "FanFolder\build-$Arch\Release\FanFolder.exe"
    if (-not (Test-Path $exe)) {
        $exe = Join-Path $root "FanFolder\build\Release\FanFolder.exe"
    }
}

if (-not (Test-Path $exe)) {
    Write-Error "FanFolder.exe not found. Build it first with Build-Installer.ps1, or pass -ExePath."
    Write-Host "  Tried: $exe"
    Write-Host "  Usage: .\Build-MSIX.ps1 -Arch x64 -ExePath 'C:\path\to\FanFolder.exe'"
    exit 1
}

Write-Host "=== Building FanFolder MSIX ($Arch) ===" -ForegroundColor Cyan
Write-Host "  Source EXE: $exe" -ForegroundColor Gray

# ── Locate Windows SDK tools ──────────────────────────────────
$sdkRoot = "C:\Program Files (x86)\Windows Kits\10\bin"

# Find latest SDK
$sdks = Get-ChildItem $sdkRoot -Directory | Where-Name -like "10.0.*" | Sort-Object Name -Descending
if ($sdks.Count -eq 0) {
    Write-Error "Windows 10 SDK not found at $sdkRoot"
    exit 1
}
$sdkVersion = $sdks[0].Name
$sdkArch = if ($Arch -eq "arm64") { "arm64" } else { "x64" }
$sdkBin = Join-Path $sdkRoot "$sdkVersion\$sdkArch"
$makeAppx = Join-Path $sdkBin "makeappx.exe"
$signTool = Join-Path $sdkBin "signtool.exe"

Write-Host "  SDK: $sdkVersion ($sdkArch)" -ForegroundColor Gray
Write-Host "  MakeAppx: $makeAppx" -ForegroundColor Gray

if (-not (Test-Path $makeAppx)) {
    # Try x86 path as fallback
    $sdkBinX86 = Join-Path $sdkRoot "$sdkVersion\x86"
    $makeAppx = Join-Path $sdkBinX86 "makeappx.exe"
    $signTool = Join-Path $sdkBinX86 "signtool.exe"
    if (-not (Test-Path $makeAppx)) {
        Write-Error "makeappx.exe not found in SDK. Install Windows 10 SDK."
        exit 1
    }
}

# ── Prepare staging directory ────────────────────────────────
$staging = Join-Path $scriptDir "staging-$Arch"
$output = Join-Path $scriptDir "output"
$msixFile = Join-Path $output "FanFolder-$Arch.msix"

# Clean previous build
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
if (Test-Path $msixFile) { Remove-Item $msixFile -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null
New-Item -ItemType Directory -Force -Path $output | Out-Null

# ── Copy files into staging ├──
# AppxManifest.xml
Copy-Item (Join-Path $scriptDir "AppxManifest.xml") $staging -Force

# FanFolder.exe
Copy-Item $exe (Join-Path $staging "FanFolder.exe") -Force

# Assets (icons) — generate from app.ico if PNGs don't exist
$assetsDir = Join-Path $staging "assets"
New-Item -ItemType Directory -Force -Path $assetsDir | Out-Null
$sourceAssetsDir = Join-Path $scriptDir "assets"
$iconPath = Join-Path $root "FanFolder\resources\app.ico"

# Required MSIX asset sizes:
#   StoreLogo.png     50x50 (or 70x70 for MS)
#   Logo44.png        44x44
#   Logo150.png       150x150
#   Logo310.png       310x310
#   Logo71.png        71x71
#   Wide310x150.png   310x150
#   SplashScreen.png  620x300

$requiredAssets = @{
    "StoreLogo.png"     = 50
    "Logo44.png"        = 44
    "Logo150.png"       = 150
    "Logo310.png"       = 310
    "Logo71.png"        = 71
    "Wide310x150.png"   = @{W=310; H=150}
    "SplashScreen.png"  = @{W=620; H=300}
}

# Check if pre-made assets exist, otherwise generate from icon
$assetsReady = $true
foreach ($name in $requiredAssets.Keys) {
    $dest = Join-Path $assetsDir $name
    $src = Join-Path $sourceAssetsDir $name
    if (Test-Path $src) {
        Copy-Item $src $dest -Force
    } elseif (Test-Path $dest) {
        # Already there from previous run
    } else {
        $assetsReady = $false
        break
    }
}

if (-not $assetsReady) {
    Write-Host "=== Generating MSIX asset PNGs from app.ico ===" -ForegroundColor Yellow
    # Use .NET to extract icon sizes from the .ico file and convert to PNG
    Add-Type -AssemblyName System.Drawing

    $ico = [System.Drawing.Icon]::new($iconPath)
    foreach ($name in $requiredAssets.Keys) {
        $size = $requiredAssets[$name]
        $dest = Join-Path $assetsDir $name

        if ($size -is [int]) {
            $w = $size; $h = $size
        } else {
            $w = $size.W; $h = $size.H
        }

        # Get the closest matching size from the multi-size icon, then resize
        $bmp = New-Object System.Drawing.Bitmap $w, $h
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.DrawImage($ico.ToBitmap(), 0, 0, $w, $h)
        $g.Dispose()

        $bmp.Save($dest, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host "  $name ($w x $h)" -ForegroundColor Gray
    }
    $ico.Dispose()
}

# ── Update version in AppxManifest if needed ───────────────────
$manifestPath = Join-Path $staging "AppxManifest.xml"
[xml]$manifest = Get-Content $manifestPath
$manifest.Package.Identity.Version = $Version
$manifest.Save($manifestPath)

# ── Build MSIX package ────────────────────────────────────────
Write-Host ""
Write-Host "=== Packing MSIX ===" -ForegroundColor Cyan
& $makeAppx pack -d $staging -p $msixFile -v
if ($LASTEXITCODE -ne 0) { throw "MakeAppx pack failed" }

Write-Host ""
Write-Host "  MSIX: $msixFile" -ForegroundColor Green
Write-Host "  Size: $([math]::Round((Get-Item $msixFile).Length / 1KB)) KB" -ForegroundColor Gray

# ── Self-sign for local testing (optional) ─────────────────────
if ($Sign) {
    Write-Host ""
    Write-Host "=== Self-signing for local test ===" -ForegroundColor Yellow

    # Create a self-signed test certificate if none exists
    $certSubject = "CN=FanFolder Test Signing"
    $certPath = Join-Path $scriptDir "FanFolderTestCert.pfx"
    $certPass = "FanFolderTest123!"

    if (-not (Test-Path $certPath)) {
        Write-Host "  Creating test certificate..." -ForegroundColor Gray
        $cert = New-SelfSignedCertificate -Type CodeSigningCert `
            -Subject $certSubject `
            -KeyUsage DigitalSignature `
            -FriendlyName "FanFolder Test Signing" `
            -CertStoreLocation "Cert:\CurrentUser\My" `
            -KeyAlgorithm RSA -KeyLength 2048 `
            -HashAlgorithm SHA256 `
            -NotAfter (Get-Date).AddYears(1)

        $secPass = ConvertTo-SecureString $certPass -Force -AsPlainText
        Export-PfxCertificate -Cert $cert -FilePath $certPath -Password $secPass
        Write-Host "  Test cert: $certPath" -ForegroundColor Gray
    }

    # Sign the MSIX
    $secPass = ConvertTo-SecureString $certPass -Force -AsPlainText
    & $signTool sign -fd SHA256 -f $certPath -p $certPass $msixFile
    if ($LASTEXITCODE -ne 0) { throw "SignTool failed" }
    Write-Host "  Signed: $msixFile" -ForegroundColor Green

    # Install cert to Trusted People so MSIX can be installed
    $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2 $certPath, $certPass
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store "TrustedPeople", "CurrentUser"
    $store.Open("ReadWrite")
    $store.Add($cert)
    $store.Close()
    Write-Host "  Certificate installed to Trusted People" -ForegroundColor Gray
}

Write-Host ""
Write-Host "=== Done ===" -ForegroundColor Green
Write-Host "  MSIX package: $msixFile" -ForegroundColor Green
Write-Host ""
Write-Host "To install for local testing:" -ForegroundColor White
Write-Host "  Add-AppxPackage -Path `"$msixFile`"" -ForegroundColor White
Write-Host ""
if (-not $Sign) {
    Write-Host "  NOTE: Package is unsigned. To self-sign for local testing:" -ForegroundColor Yellow
    Write-Host "  .\Build-MSIX.ps1 -Arch x64 -Sign" -ForegroundColor Yellow
}
