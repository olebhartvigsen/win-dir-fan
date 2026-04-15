<#
.SYNOPSIS
    Installs FanFolder to the user's local app directory.
.DESCRIPTION
    Copies FanFolder.exe to %LocalAppData%\FanFolder,
    creates Start Menu and optional Desktop shortcuts, and optionally adds
    a startup registry entry.
#>
param(
    [switch]$AddToStartup,
    [switch]$Uninstall,
    [switch]$Silent
)

$AppName      = "FanFolder"
$ExeName      = "FanFolder.exe"
$InstallDir   = Join-Path $env:LOCALAPPDATA "FanFolder"
$StartMenuDir = Join-Path ([Environment]::GetFolderPath("StartMenu")) "Programs"
$DesktopDir   = [Environment]::GetFolderPath("Desktop")
$RegPath      = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$RegName      = "FanFolder"

# Resolve script directory reliably
if ($PSScriptRoot) {
    $SourceDir = $PSScriptRoot
} elseif ($MyInvocation.MyCommand.Path) {
    $SourceDir = Split-Path -Parent $MyInvocation.MyCommand.Path
} else {
    $SourceDir = (Get-Location).Path
}

# ── Uninstall ────────────────────────────────────────────────
if ($Uninstall) {
    Write-Host "Uninstalling $AppName..." -ForegroundColor Yellow

    $p = Get-Process | Where-Object { $_.Name -like "*FanFolder*" }
    if ($p) { Stop-Process -Id $p.Id; Start-Sleep -Milliseconds 500 }

    if (Test-Path $InstallDir) { Remove-Item $InstallDir -Recurse -Force }

    $lnk = Join-Path $StartMenuDir "$AppName.lnk"
    if (Test-Path $lnk) { Remove-Item $lnk -Force }

    $deskLnk = Join-Path $DesktopDir "$AppName.lnk"
    if (Test-Path $deskLnk) { Remove-Item $deskLnk -Force }

    Remove-ItemProperty -Path $RegPath -Name $RegName -ErrorAction SilentlyContinue

    Write-Host "$AppName has been uninstalled." -ForegroundColor Green
    if (-not $Silent) { Read-Host "Press Enter to close" }
    exit 0
}

# ── Install ──────────────────────────────────────────────────
Write-Host "Installing $AppName..." -ForegroundColor Cyan

# Stop running instance
$p = Get-Process | Where-Object { $_.Name -like "*FanFolder*" }
if ($p) { Stop-Process -Id $p.Id; Start-Sleep -Milliseconds 500 }

# Create install directory
if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# Locate the exe — prefer Release build next to this script, then repo build output
$buildExe = Join-Path $SourceDir $ExeName
if (-not (Test-Path $buildExe)) {
    $buildExe = Join-Path $SourceDir "..\FanFolder\build\Release\$ExeName"
}

if (-not (Test-Path $buildExe)) {
    Write-Host "ERROR: Cannot find $ExeName. Build the project first:" -ForegroundColor Red
    Write-Host "  cmake --build FanFolder\build --config Release" -ForegroundColor Yellow
    if (-not $Silent) { Read-Host "Press Enter to close" }
    exit 1
}

Copy-Item $buildExe (Join-Path $InstallDir $ExeName) -Force
Write-Host "  Copied $ExeName" -ForegroundColor Gray

# Start Menu shortcut
try {
    $shell = New-Object -ComObject WScript.Shell

    $startLnkPath = Join-Path $StartMenuDir "$AppName.lnk"
    $startLnk = $shell.CreateShortcut($startLnkPath)
    $startLnk.TargetPath = Join-Path $InstallDir $ExeName
    $startLnk.WorkingDirectory = $InstallDir
    $startLnk.Description = $AppName
    $startLnk.Save()
    Write-Host "  Created Start Menu shortcut" -ForegroundColor Green

    $deskLnkPath = Join-Path $DesktopDir "$AppName.lnk"
    $deskLnk = $shell.CreateShortcut($deskLnkPath)
    $deskLnk.TargetPath = Join-Path $InstallDir $ExeName
    $deskLnk.WorkingDirectory = $InstallDir
    $deskLnk.Description = $AppName
    $deskLnk.Save()
    Write-Host "  Created Desktop shortcut" -ForegroundColor Green

    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($shell) | Out-Null
} catch {
    Write-Host "  WARNING: Failed to create shortcuts: $_" -ForegroundColor Yellow
}

# Startup with Windows
if ($AddToStartup) {
    $exePath = Join-Path $InstallDir $ExeName
    Set-ItemProperty -Path $RegPath -Name $RegName -Value "`"$exePath`""
    Write-Host "  Added to Windows startup" -ForegroundColor Gray
} else {
    Remove-ItemProperty -Path $RegPath -Name $RegName -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "$AppName installed to: $InstallDir" -ForegroundColor Green
Write-Host ""

if (-not $Silent) {
    $launch = Read-Host "Launch now? (Y/n)"
    if ($launch -ne 'n') {
        Start-Process (Join-Path $InstallDir $ExeName)
    }
}

