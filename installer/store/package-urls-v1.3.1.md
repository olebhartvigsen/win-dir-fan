# FanFolder v1.3.1 — Package URLs for Store Submission

## Package URLs (redirect-free)

Partner Center rejects URLs that redirect (302). GitHub Releases URLs redirect to `release-assets.githubusercontent.com`, so we host the MSI files directly in the `olebhartvigsen/FanFolder` repo under `downloads/v1.3.1/` and serve them via `raw.githubusercontent.com` (HTTP 200, no redirect, `application/octet-stream`).

### x64
- **Package URL:** `https://raw.githubusercontent.com/olebhartvigsen/FanFolder/main/downloads/v1.3.1/FanFolderSetup-x64.msi`
- **Architecture:** x64
- **App type:** MSI (Store applies `/qn` automatically)
- **Installer params:** _(leave empty)_
- **Size:** 1,228,800 bytes (1.2 MB)

### ARM64
- **Package URL:** `https://raw.githubusercontent.com/olebhartvigsen/FanFolder/main/downloads/v1.3.1/FanFolderSetup-arm64.msi`
- **Architecture:** arm64
- **App type:** MSI (Store applies `/qn` automatically)
- **Installer params:** _(leave empty)_
- **Size:** 1,167,360 bytes (1.1 MB)

## Verification (2026-08-03)

Both URLs verified:
- HTTP 200 (no redirect)
- Content-Type: `application/octet-stream`
- Content-Length matches actual file size
- Accessible via HTTPS (TLS 1.2+)

## Why MSI not EXE?

The Store runs MSI installers silently with the built-in `/qn` switch. No extra installer parameters or return code handling is needed. With EXE you'd need to specify custom `/S` or `/quiet` parameters and map return codes.

## GitHub Pages alternative

The files are also served via GitHub Pages at:
- `https://olebhartvigsen.github.io/FanFolder/downloads/v1.3.1/FanFolderSetup-x64.msi`
- `https://olebhartvigsen.github.io/FanFolder/downloads/v1.3.1/FanFolderSetup-arm64.msi`

Both work (HTTP 200, no redirect), but `raw.githubusercontent.com` is preferred as it's purpose-built for serving raw file content.
