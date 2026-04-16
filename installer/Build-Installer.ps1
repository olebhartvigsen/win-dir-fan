# Build-Installer.ps1
# Builds FanFolder release exe (x64 + arm64) and packages into a combined installer
#
# Prerequisites:
#   - Visual Studio Build Tools with CMake + ARM64 build tools
#   - .NET SDK 9+
#   - dotnet tool install --global wix --version 4.0.5
#   - wix extension add WixToolset.UI.wixext/4.0.5   (run once from installer/ dir)
#   - wix extension add WixToolset.Bal.wixext/4.0.5
#   - wix extension add WixToolset.Util.wixext/4.0.5

$ErrorActionPreference = "Stop"
$root    = Split-Path -Parent $PSScriptRoot
$cmake   = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

$archs = @(
    @{ Name = "x64";   CMakeArch = "x64" },
    @{ Name = "arm64"; CMakeArch = "ARM64" }
)

foreach ($arch in $archs) {
    $name     = $arch.Name
    $buildDir = "$root\FanFolder\build-$name"

    Write-Host "=== Configuring FanFolder ($name) ===" -ForegroundColor Cyan
    & $cmake -B $buildDir -G "Visual Studio 17 2022" -A $arch.CMakeArch -S "$root\FanFolder"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($name)" }

    Write-Host "=== Building FanFolder ($name Release) ===" -ForegroundColor Cyan
    & $cmake --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw "CMake build failed ($name)" }
    Write-Host "  OK: $buildDir\Release\FanFolder.exe" -ForegroundColor Green

    # Copy exe to artifacts layout expected by bundle
    $artifactDir = "$root\artifacts\$name\FanFolder\build\Release"
    New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
    Copy-Item "$buildDir\Release\FanFolder.exe" "$artifactDir\FanFolder.exe" -Force

    Write-Host "=== Building MSI ($name) ===" -ForegroundColor Cyan
    Set-Location $PSScriptRoot
    dotnet build FanFolder.wixproj -c Release -p:Platform=$name -p:OutputName="FanFolderSetup-$name"
    if ($LASTEXITCODE -ne 0) { throw "MSI build failed ($name)" }

    # Copy MSI to artifacts layout expected by bundle
    $msiArtifactDir = "$root\artifacts\$name\installer\output"
    New-Item -ItemType Directory -Force -Path $msiArtifactDir | Out-Null
    Copy-Item "$PSScriptRoot\output\FanFolderSetup-$name.msi" "$msiArtifactDir\FanFolderSetup-$name.msi" -Force

    Write-Host "  OK: FanFolderSetup-$name.msi" -ForegroundColor Green
}

Write-Host ""
Write-Host "=== Building combined installer (Burn bundle) ===" -ForegroundColor Cyan
Set-Location "$PSScriptRoot\bundle"
dotnet build FanFolderBundle.wixproj -c Release
if ($LASTEXITCODE -ne 0) { throw "Bundle build failed" }

$bundle = "$PSScriptRoot\bundle\output\FanFolderSetup.exe"
Write-Host ""
Write-Host "Combined installer ready: $bundle" -ForegroundColor Green
Write-Host "Size: $([math]::Round((Get-Item $bundle).Length / 1KB)) KB" -ForegroundColor Gray
