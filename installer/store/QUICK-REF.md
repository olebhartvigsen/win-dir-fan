# FanFolder v1.3.1 Upload — Quick Reference

## Files (CI Build 30891887551)
```
FanFolder-x64.msix    (1220 KB)
FanFolder-arm64.msix  (1163 KB)
```

## Upload Steps

### 1. Partner Center
https://partner.microsoft.com/en-us/dashboard/win32apps/00765a5e-5d0b-4f78-af6f-bb51b3b0379b/overview

### 2. Start Submission
- Version: **1.3.1.0**
- Upload: **both .msix files**
- Microsoft signs automatically

### 3. Store Listing Changes
**What's new:**
> FanFolder is now available as an MSIX package for the Microsoft Store. Supports x64 and ARM64 architectures. Native Win32 utility — no sandbox limitations.

### 4. Capability Justification (FORM FIELD)
> **runFullTrust**: Required as FanFolder is a system tray utility with global hotkey support that must run as a full Win32 process outside the UWP sandbox.
>
> **broadFileSystemAccess**: Required to scan user-configured root directories for recent file references. The user explicitly selects these paths in settings. No personal data or file contents are transmitted externally.
>
> **windows.startupTask**: Required so the tray utility and fan popup are available immediately after Windows login.

### 5. System Requirements
- OS: Windows 10 version 1903+ or Windows 11
- Architecture: x64, ARM64

### 6. Expected Timeline
- Validation: 5-30 min
- Review: 2-7 business days
- Result: APPROVAL (standard Desktop Bridge app)

## If Rejected
- Common: Missing capability justification → fill Section 4 above
- Escalate: Reference Desktop Bridge documentation
- Rollback: Keep v1.2.x as fallback

## Support
Full guide: `installer/store/SUBMIT-v1.3.1.md`
CI Build: `30891887551` (SHA `8fa0790`)
