# Build-Installer.ps1
# Builds FanFolder release exe and packages it into FanFolderSetup.msi
#
# Prerequisites:
#   - Visual Studio Build Tools with CMake
#   - .NET SDK 9+
#   - dotnet tool install --global wix --version 4.0.5
#   - wix extension add WixToolset.UI.wixext/4.0.5  (run once from installer/ dir)

$ErrorActionPreference = "Stop"
$root    = Split-Path -Parent $PSScriptRoot
$cmake   = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$buildDir = "$root\FanFolder\build"

Write-Host "=== Building FanFolder (Release) ===" -ForegroundColor Cyan
& $cmake --build $buildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }
Write-Host "  OK: $buildDir\Release\FanFolder.exe" -ForegroundColor Green

Write-Host ""
Write-Host "=== Building MSI ===" -ForegroundColor Cyan
Set-Location $PSScriptRoot
dotnet build FanFolder.wixproj -c Release
if ($LASTEXITCODE -ne 0) { throw "MSI build failed" }

$msi = "$PSScriptRoot\output\FanFolderSetup.msi"
Write-Host ""
Write-Host "MSI ready: $msi" -ForegroundColor Green
Write-Host "Size: $([math]::Round((Get-Item $msi).Length / 1KB)) KB" -ForegroundColor Gray
