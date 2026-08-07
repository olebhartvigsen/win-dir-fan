# Test CI-built MSIX locally
# Run this in PowerShell as Administrator (for Developer Mode sideloading)

$MsixPath = "C:\Users\au19277\Downloads\FanFolder-x64-CI.msix"

Write-Host "=== FanFolder CI MSIX Test ===" -ForegroundColor Cyan

# 1. Check Developer Mode
$devMode = Get-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock" -Name "AllowDevelopmentWithoutDevLicense" -ErrorAction SilentlyContinue
if ($devMode.AllowDevelopmentWithoutDevLicense -eq 1) {
    Write-Host "✓ Developer Mode: ENABLED" -ForegroundColor Green
} else {
    Write-Host "✗ Developer Mode: DISABLED - sideloading may fail" -ForegroundColor Red
}

# 2. Uninstall existing FanFolder packages
$existing = Get-AppxPackage -Name "*FanFolder*"
if ($existing) {
    Write-Host "Removing existing package: $($existing.PackageFullName)" -ForegroundColor Yellow
    Remove-AppxPackage -Package $existing.PackageFullName
}

# 3. Install the MSIX
Write-Host "Installing $MsixPath..." -ForegroundColor Cyan
Add-AppxPackage -Path $MsixPath

# 4. Verify installed package
$pkg = Get-AppxPackage -Name "*FanFolder*"
if ($pkg) {
    Write-Host "✓ Installed: $($pkg.PackageFullName)" -ForegroundColor Green
    Write-Host "  Version: $($pkg.Version)"
    Write-Host "  Location: $($pkg.InstallLocation)"
    
    # 5. Check EXE file version
    $exe = Join-Path $pkg.InstallLocation "FanFolder.exe"
    if (Test-Path $exe) {
        $verInfo = (Get-Item $exe).VersionInfo
        Write-Host "  EXE FileVersion: $($verInfo.FileVersion)"
        Write-Host "  EXE ProductVersion: $($verInfo.ProductVersion)"
    }
    
    # 6. Launch and verify
    Write-Host "`nStarting FanFolder..." -ForegroundColor Cyan
    Start-Process $exe
    Start-Sleep -Seconds 3
    $proc = Get-Process -Name "FanFolder" -ErrorAction SilentlyContinue
    if ($proc) {
        Write-Host "✓ Running: PID $($proc.Id)" -ForegroundColor Green
    }
} else {
    Write-Host "✗ Installation failed - package not found" -ForegroundColor Red
}

Write-Host "`n=== Test Complete ===" -ForegroundColor Cyan
