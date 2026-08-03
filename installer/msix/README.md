# FanFolder MSIX Package — Local Test Build

## Quick Start

### 1. Build FanFolder.exe (if not already built)

```powershell
# From win-dir-fan root:
.\installer\Build-Installer.ps1
# Or just build the exe:
cmake -B FanFolder\build -G "Visual Studio 17 2022" -A x64 -S FanFolder
cmake --build FanFolder\build --config Release
```

### 2. Build MSIX package

```powershell
# From installer\msix\:
.\Build-MSIX.ps1 -Arch x64 -Sign

# Or from root:
.\installer\msix\Build-MSIX.ps1 -Arch x64 -Sign
```

The `-Sign` flag creates a self-signed test certificate and signs the MSIX so you can install it locally.

### 3. Install for testing

```powershell
# Install the MSIX:
Add-AppxPackage -Path "installer\msix\output\FanFolder-x64.msix"

# Uninstall:
Remove-AppxPackage -Package (Get-AppxPackage FanFolder).PackageFullName
```

Or double-click the `.msix` file in Explorer to use the Windows installer dialog.

## Output

```
installer/msix/
├── AppxManifest.xml     # MSIX manifest (identity, capabilities, extensions)
├── Build-MSIX.ps1       # Build script
├── assets/              # Store logos and tile images (auto-generated from app.ico)
├── staging-x64/         # Working directory during build (gitignored)
└── output/
    └── FanFolder-x64.msix   # Final package
```

## How It Works

The script:
1. Copies the pre-built `FanFolder.exe` and `AppxManifest.xml` into a staging directory
2. Auto-generates the required Store logo PNGs (44x44, 50x50, 71x71, 150x150, 310x310, 310x150, 620x300 splash) from `FanFolder\resources\app.ico` using .NET System.Drawing
3. Runs `makeappx.exe pack` to create the `.msix` package
4. With `-Sign`: creates a self-signed SHA256 code-signing cert, signs the MSIX, and installs the cert to `Trusted People` so the package can be installed

## MSIX Manifest Highlights

- **Identity:** `OleBhartvigsen.FanFolder`, Publisher `CN=Ole Bhartvigsen, O=Ole Bhartvigsen, C=DK`
- **Full trust:** `runFullTrust` capability — runs as a normal Win32 app, not sandboxed
- **File system:** `broadFileSystemSystem` — full file access like a normal desktop app
- **Autostart:** `desktop4:StartupTask` extension — FanFolder can run at login
- **Min version:** Windows 10 2004 (10.0.19041) — first version with full MSIX desktop support

## Important Notes

- **Self-signed certs are for LOCAL TESTING ONLY** — the Store requires a real certificate from a CA in the Microsoft Trusted Root Program, or Microsoft Trusted Signing (Azure, $9.99/month)
- **Before installing**, the test certificate must be in `Trusted People` — the `-Sign` flag does this automatically
- If Windows says "this app can't run on your PC", enable Developer Mode or Sideloading:
  - Settings → Update & Security → For developers → Developer Mode (recommended)
  - Or: enable "Install apps from any source" (sideloading)

## For Store Submission (later)

For the actual Store submission, you would either:
1. Use Microsoft Trusted Signing (Azure) to sign the MSIX with a trusted cert, or
2. Submit the MSIX to the Store — Microsoft signs it for you during certification

But for local testing first, use the `-Sign` self-signed approach.
