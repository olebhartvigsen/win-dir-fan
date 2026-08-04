# FanFolder v1.3.1 — Package URLs for Store Submission (MSIX)

## Package URLs (redirect-free)

Partner Center rejects URLs that redirect (302). GitHub Releases URLs redirect to `release-assets.githubusercontent.com`, so we host the MSIX files directly in the `olebhartvigsen/FanFolder` repo under `downloads/v1.3.1/` and serve them via `raw.githubusercontent.com` (HTTP 200, no redirect, `application/octet-stream`).

### x64
- **Package URL:** `https://raw.githubusercontent.com/olebhartvigsen/FanFolder/main/downloads/v1.3.1/FanFolder-x64.msix`
- **Architecture:** x64
- **App type:** MSIX
- **Installer params:** _(leave empty)_
- **Size:** 1,250,224 bytes (1.2 MB)

### ARM64
- **Package URL:** `https://raw.githubusercontent.com/olebhartvigsen/FanFolder/main/downloads/v1.3.1/FanFolder-arm64.msix`
- **Architecture:** arm64
- **App type:** MSIX
- **Installer params:** _(leave empty)_
- **Size:** 1,191,783 bytes (1.1 MB)

## Verification (2026-08-04)

Both URLs verified:
- HTTP 200 (no redirect)
- Content-Type: `application/octet-stream`
- Content-Length matches actual file size
- Accessible via HTTPS (TLS 1.2+)

## MSIX Manifest Compliance

Both packages comply with Microsoft Store [app-package-requirements](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/app-package-requirements):
- Version: 1.3.1.0 (4-part, last=0)
- MinVersion: 10.0.19041.0
- MaxVersionTested: 10.0.26100.0
- TargetDeviceFamily: Windows.Desktop
- BlockMap: SHA-256
- Filenames: ANSI only
- Capabilities: runFullTrust, broadFileSystemAccess
- EntryPoint: Windows.FullTrustApplication

CI Build: `30901828284` (SHA `fdc8731`)
