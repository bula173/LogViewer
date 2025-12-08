# Code Signing Setup

This directory contains scripts and configuration for code signing Windows executables.

## ⚠️ Important Security Notice

**Self-signed certificates are for DEVELOPMENT and TESTING ONLY!**

- They will show security warnings to end users
- They provide NO security benefits for distribution
- For production software, use certificates from trusted Certificate Authorities

## Quick Start

### 1. Prerequisites

Install Windows SDK (includes `makecert.exe` and `pvk2pfx.exe`):
- Download from: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
- Or install Visual Studio with C++ development tools

### 2. Create Self-Signed Certificate

Run one of these scripts:

**Batch script:**
```cmd
create_self_signed_cert.bat
```

**PowerShell script:**
```powershell
.\create_self_signed_cert.ps1 -CertName "MyAppDev" -Password "mysecurepassword"
```

This creates:
- `LogViewerDev.pfx` - The certificate file for signing
- `LogViewerDev.cer` - Certificate only
- `LogViewerDev.pvk` - Private key

### 3. Configure CMake

```cmake
cmake -DCODE_SIGN_CERTIFICATE="C:/path/to/LogViewerDev.pfx" \
      -DCODE_SIGN_PASSWORD="password123" \
      ..
```

### 4. Build

```cmake
cmake --build . --config Release
```

The executable will be automatically signed during the build process.

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