# Code Signing Setup

This directory contains scripts and configuration for code signing on Windows and macOS,
including automated signing via GitHub Actions.

## GitHub Actions — Automated Signing (Recommended)

The `release.yml` workflow signs binaries automatically when the relevant repository
secrets are configured. Signing steps are skipped gracefully when secrets are absent,
so unsigned release builds still work without any secret configuration.

### Required GitHub Secrets

Navigate to **Settings → Secrets and variables → Actions** in the repository, then add:

#### macOS signing

| Secret | Description |
|---|---|
| `MACOS_CERTIFICATE` | Base64-encoded Developer ID Application `.p12` certificate |
| `MACOS_CERTIFICATE_PWD` | Password for the `.p12` file |
| `MACOS_CERTIFICATE_NAME` | Full certificate CN, e.g. `Developer ID Application: Name (TEAMID)` |
| `MACOS_KEYCHAIN_PWD` | Any strong random password used for the temporary CI keychain |
| `APPLE_ID` | Apple ID email used for notarization |
| `APPLE_TEAM_ID` | 10-character Apple Team ID |
| `APPLE_APP_SPECIFIC_PASSWORD` | App-specific password generated at appleid.apple.com |

**Export your certificate:**
```bash
# On your Mac, export from Keychain Access as .p12, then encode:
base64 -i DeveloperID.p12 | pbcopy   # copies base64 to clipboard
```

**Notarization** is only attempted when `APPLE_ID` is set and requires a valid
Developer ID Application certificate (not just a self-signed one).

**Without `MACOS_CERTIFICATE` set**, the build falls back to ad-hoc signing
(`codesign --sign -`) — this keeps the bundle's own signature internally
consistent (useful after `macdeployqt` copies in Qt frameworks) but does
**not** satisfy Gatekeeper. Downloaded ad-hoc-signed and unsigned builds show
the same "Apple could not verify... is free of malware" prompt; only a real
Developer ID certificate plus notarization removes it. There is no
self-signed-certificate equivalent of the Windows path on macOS — Gatekeeper
only trusts certificates chained to Apple's own CA, which requires a paid
Apple Developer Program membership.

#### Windows signing

| Secret | Description |
|---|---|
| `WINDOWS_CERTIFICATE` | Base64-encoded code-signing `.pfx` certificate |
| `WINDOWS_CERTIFICATE_PWD` | Password for the `.pfx` file |

**Encode your certificate:**
```powershell
[Convert]::ToBase64String([IO.File]::ReadAllBytes("cert.pfx")) | Set-Clipboard
```

Both the `LogViewer.exe` executable and the NSIS installer are signed when these
secrets are present. SHA-256 digest + RFC 3161 timestamping via DigiCert is used.

---

## ⚠️ Important Security Notice

**Self-signed certificates are for DEVELOPMENT and TESTING ONLY!**

- Windows SmartScreen is reputation-based, not signature-based — a self-signed
  certificate does **not** stop the "Windows protected your PC" prompt on
  first downloads (only a paid EV certificate gets instant reputation; a
  standard paid certificate still has to build reputation over time). What it
  *does* give you: a named publisher instead of "Unknown publisher", and
  tamper-evidence (the signature breaks if the binary is modified after
  signing).
- For production software, use a certificate from a trusted Certificate
  Authority.

## Quick Start

### 1. Prerequisites

None beyond PowerShell (built into Windows 10/11). The cert-generation script
uses the built-in `New-SelfSignedCertificate` cmdlet — no Windows SDK download
needed. (An older version of this script used `makecert.exe`/`pvk2pfx.exe`,
which Microsoft has since removed from current Windows SDK releases.)

### 2. Create Self-Signed Certificate

```powershell
.\create_self_signed_cert.ps1 -CertName "MyAppDev" -Password "mysecurepassword"
```

(`create_self_signed_cert.bat` is a thin wrapper around the same script, for
double-click convenience.)

This creates `LogViewerDev.pfx` — the certificate + private key, used for
signing.

### 3a. Local builds (CMake)

```cmake
cmake -DCODE_SIGN_CERTIFICATE="C:/path/to/LogViewerDev.pfx" \
      -DCODE_SIGN_PASSWORD="password123" \
      ..
cmake --build . --config Release
```

The executable is signed automatically during the build.

### 3b. CI builds (GitHub Actions release.yml)

Release builds are signed automatically when the `WINDOWS_CERTIFICATE` /
`WINDOWS_CERTIFICATE_PWD` repo secrets are set (see "Required GitHub Secrets"
above) — a self-signed `.pfx` works exactly the same way a commercial one
does here, it's just a `.pfx` + password either way. Base64-encode it:

```powershell
[Convert]::ToBase64String([IO.File]::ReadAllBytes("LogViewerDev.pfx")) | Set-Clipboard
```

then paste as `WINDOWS_CERTIFICATE`, and set `WINDOWS_CERTIFICATE_PWD` to the
password you passed to the script.

## Certificate Types

### Development (Self-Signed)
- ✅ Free
- ✅ Easy to create
- ❌ Shows security warnings
- ❌ Not trusted by Windows SmartScreen

### Production (Commercial CA)
- ✅ Trusted by all Windows versions
- ✅ No security warnings
- ✅ Required for Microsoft Store
- ❌ Costs money ($200-500/year)

## Commercial Certificate Authorities

- **DigiCert**: https://www.digicert.com/code-signing/
- **GlobalSign**: https://www.globalsign.com/code-signing/
- **Sectigo**: https://sectigo.com/ssl-certificates/code-signing
- **Entrust**: https://www.entrust.com/code-signing

## Troubleshooting

### "signtool.exe not found"
Install Windows SDK or Visual Studio with Windows development tools.

### "Access denied" when signing
- Run command prompt as Administrator
- Check certificate file permissions
- Verify password is correct

### Certificate appears invalid
- Check system date/time (certificates are time-sensitive)
- Verify certificate hasn't expired
- Use `/v` flag with signtool for verbose output

## Advanced Configuration

### Custom Timestamp Server
```cmake
cmake -DCODE_SIGN_TIMESTAMP_URL="http://timestamp.globalsign.com/tsa/r6advanced1" ..
```

### EV Code Signing (Hardware Token Required)
For kernel drivers and highest trust level, use Extended Validation certificates with hardware security modules.

## Security Best Practices

1. **Store certificates securely** - Use hardware security modules for production
2. **Use strong passwords** - Minimum 12 characters with special symbols
3. **Regular renewal** - Certificates expire (typically 1-3 years)
4. **Separate dev/prod certificates** - Never use production certificates for development
5. **Audit signing operations** - Log who signed what and when

## Files in this Directory

- `create_self_signed_cert.bat` - Batch script for certificate creation
- `create_self_signed_cert.ps1` - PowerShell script for certificate creation
- `CODE_SIGNING_README.md` - This documentation
- `LogViewer.rc.in` - Windows resource template (auto-versioned)
- `LogViewer.manifest` - Application manifest (auto-versioned)