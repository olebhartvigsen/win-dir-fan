# FanFolder v1.3.1 Microsoft Store Submission Guide

## Overview
- **Product ID**: `00765a5e-5d0b-4f78-af6f-bb51b3b0379b`
- **Package Format**: MSIX (x64 + ARM64)
- **Identity**: `OleBhartvigsen.FanFolder`
- **Version**: 1.3.1.0
- **Publisher**: `CN=Ole Bhartvigsen, O=Ole Bhartvigsen, C=DK`

## Files to Upload

### Primary MSIX Packages (from CI build)
| Architecture | File | Size | Source |
|---|---|---|---|
| x64 | `FanFolder-x64.msix` | ~1220 KB | CI Job `build-msix` |
| ARM64 | `FanFolder-arm64.msix` | ~1163 KB | CI Job `build-msix` |

### Backup Files (if needed)
- Build artifacts from GitHub Actions run `30891887551`
- Download: `gh run download 30891887551 --repo olebhartvigsen/win-dir-fan --name msix-packages`

## Partner Center Submission Steps

### 1. Navigate to Product
- URL: https://partner.microsoft.com/en-us/dashboard/win32apps/00765a5e-5d0b-4f78-af6f-bb51b3b0379b/overview
- Or: Apps and games → FanFolder → Product overview

### 2. Create Submission
- Click "Start your submission" or "Update"
- Version: `1.3.1.0`
- Target release: Immediate (or scheduled)

### 3. Packages Section
- Upload BOTH `.msix` files
- Microsoft will sign them automatically
- **Do NOT upload an MSI** — only MSIX

### 4. Store Listings
Reuse existing listing from v1.2.x, update version notes:
- **What's new in this version**: see `release-notes.md` below

### 5. Capabilities Justification (CRITICAL)
When prompted, provide this explanation:

```
FanFolder is a system utility that runs in the Windows tray area
and provides quick access to recent folders and user-defined root  
directories. 

Required capabilities:
- runFullTrust: FanFolder is a native Win32 application (tray util 
  with hotkey support). It cannot function as a sandboxed UWP app.
  
- broadFileSystemAccess: Required to scan user-designated "stammapper"  
  (root folder paths) for recent file changes. The user explicitly  
  configures these paths in settings. No data leaves the device.
  
- windows.startupTask: Tray utility that should start with Windows  
  so the fan popup and hotkey are available immediately.
```

### 6. System Requirements
- **Min OS Version**: Windows 10, version 1903 (build 18362)
- **Recommended**: Windows 10 version 2004+ or Windows 11
- **Architecture**: x64, ARM64
- **No special hardware**

### 7. Age Rating
- Category: Productivity/Utilities
- Content: Suitable for all ages (3+)
- No violence, gambling, or mature content

### 8. Certification Notes (Optional but recommended)
```
This is an MSIX Desktop Bridge app (runFullTrust). 
It was previously submitted as MSI but was rejected under 
policy 10.2.9. We have now correctly packaged it as MSIX 
with the required capabilities fully declared.
```

---

## Release Notes (for Store listing)

### What's new in v1.3.1
- Now packaged as MSIX for Microsoft Store distribution
- Supports both x64 and ARM64 architectures
- Native Win32 tray utility — no sandbox limitations
- Quick fan popup for recent folders and configured root paths
- Configurable startup with Windows
- Danish and international language support

### Technical Details
- Built with Visual Studio 2022 / Windows SDK 10.0.26100
- Uses CMake build system with vcpkg dependencies
- Self-contained — no external runtime dependencies

---

## Pre-Submission Checklist

- [ ] MSIX files downloaded from CI (run `30891887551`)
- [ ] Packages are named `FanFolder-x64.msix` and `FanFolder-arm64.msix`
- [ ] Manifest version is `1.3.1.0` in both packages
- [ ] Publisher matches Partner Center identity: `CN=Ole Bhartvigsen...`
- [ ] `runFullTrust` capability declared (required for manual review)
- [ ] `broadFileSystemAccess` declared with justification
- [ ] `windows.startupTask` declared with uap5 namespace
- [ ] Logo 1080x1080 uploaded (or existing from prior submission)
- [ ] 28 languages configured with localized descriptions
- [ ] Screenshots uploaded (Danish + English minimum)
- [ ] Privacy URL: https://olebhartvigsen.github.io/FanFolder/PRIVACY.md
- [ ] Support contact: [your email]

---

## Expected Timeline
- **Automated validation**: 5-30 minutes (makeappx validation)
- **Manual review**: 2-7 business days
- **Possible issue**: If reviewer rejects runFullTrust, escalate with   
  documentation that it's a genuine system utility, not a game/entertainment app
- **Expected outcome**: Approval — this is a standard Desktop Bridge app

---

## Troubleshooting Upload

### Error: "Package format is not valid"
- Cause: Corrupted upload or wrong file extension
- Fix: Re-download from CI artifacts, verify with `Expand-Archive`

### Error: "Signature invalid"
- Cause: You tried to sign it yourself or upload .zip renamed to .msix
- Fix: Upload the RAW CI `.msix` file. MS Store signs it automatically.

### Error: "Capabilities not declared"
- Cause: Missing capability justification text
- Fix: Fill the form as shown in Section 5 above

---

## Verification After Upload
After clicking "Submit to Store":
1. Watch email for certification feedback
2. If rejected, check exact policy number and reason
3. Most common fix needed: enhance capability justification
4. Re-submit within 24 hours if only text changes needed

## Rollback Plan
If v1.3.1 fails:
- Keep v1.2.x as fallback in Partner Center
- Can immediately "Stop submission" before approval
- Rollback to prior package version if already published

---

*Generated: 2026-08-04*
*CI Build: `30891887551` (SHA `8fa0790`)*
*Source: https://github.com/olebhartvigsen/win-dir-fan*
