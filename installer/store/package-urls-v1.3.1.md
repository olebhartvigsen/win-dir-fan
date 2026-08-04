# FanFolder v1.3.1 — Package URLs for Store Submission (MSIX)

## Package URLs (redirect-free)

Partner Center rejects URLs that redirect (302). GitHub Releases URLs redirect to `release-assets.githubusercontent.com`, so we host the MSIX files directly in the `olebhartvigsen/FanFolder` repo under `downloads/v1.3.1/` and serve them via `raw.githubusercontent.com` (HTTP 200, no redirect, `application/octet-stream`).

### x64
- **Package URL:** `https://raw.githubusercontent.com/olebhartvigsen/FanFolder/main/downloads/v1.3.1/FanFolder-x64.msix`
- **Architecture:** x64
- **App type:** MSIX
- **Installer params:** _(leave empty)_
- **Size:** 1,250,209 bytes (1.2 MB)

### ARM64
- **Package URL:** `https://raw.githubusercontent.com/olebhartvigsen/FanFolder/main/downloads/v1.3.1/FanFolder-arm64.msix`
- **Architecture:** arm64
- **App type:** MSIX
- **Installer params:** _(leave empty)_
- **Size:** 1,191,778 bytes (1.1 MB)

## Verification (2026-08-04)

Both URLs verified:
- HTTP 200 (no redirect)
- Content-Type: `application/octet-stream`
- Content-Length matches actual file size
- Accessible via HTTPS (TLS 1.2+)
