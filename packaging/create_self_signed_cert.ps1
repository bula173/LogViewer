# Self-Signed Code Signing Certificate Creation Script
# WARNING: This creates a DEVELOPMENT-ONLY certificate
# DO NOT use for production/distribution

param(
    [string]$CertName = "LogViewerDev",
    [string]$Password = "password123"
)

Write-Host "Creating self-signed code signing certificate..." -ForegroundColor Yellow
Write-Host "WARNING: A self-signed certificate does NOT eliminate Windows SmartScreen" -ForegroundColor Red
Write-Host "warnings - SmartScreen is reputation-based, not signature-based. It gives" -ForegroundColor Red
Write-Host "you a named publisher and tamper-evidence, not a 'trusted' badge." -ForegroundColor Red
Write-Host ""

# New-SelfSignedCertificate is a built-in PowerShell PKI cmdlet (Windows 10/11,
# Server 2016+) - no Windows SDK / makecert.exe download required. makecert.exe
# and pvk2pfx.exe (used by the previous version of this script) were removed
# from current Windows SDK releases, so that approach no longer works on a
# freshly-installed machine.
$cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject "CN=$CertName" `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -HashAlgorithm SHA256 `
    -NotAfter (Get-Date).AddYears(3)

if (-not $cert) {
    Write-Host "ERROR: Failed to create certificate" -ForegroundColor Red
    exit 1
}

Write-Host "Certificate created in the current user's certificate store."
Write-Host "Exporting to PFX..."

$securePassword = ConvertTo-SecureString -String $Password -Force -AsPlainText
$pfxPath = Join-Path $PWD "$CertName.pfx"
Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $securePassword | Out-Null

# Remove the certificate from the store now that it's exported - the .pfx is
# the portable artifact; leaving a duplicate copy in the store isn't needed.
Remove-Item -Path "Cert:\CurrentUser\My\$($cert.Thumbprint)" -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Certificate created successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "File created:" -ForegroundColor Cyan
Write-Host "  $pfxPath"
Write-Host ""
Write-Host "For local builds (CMake):" -ForegroundColor Cyan
Write-Host "  -DCODE_SIGN_CERTIFICATE=`"$pfxPath`""
Write-Host "  -DCODE_SIGN_PASSWORD=`"$Password`""
Write-Host ""
Write-Host "For CI (GitHub Actions release.yml), base64-encode it and store as repo secrets:" -ForegroundColor Cyan
Write-Host "  [Convert]::ToBase64String([IO.File]::ReadAllBytes(`"$pfxPath`")) | Set-Clipboard"
Write-Host "  Then set WINDOWS_CERTIFICATE (the base64 text) and WINDOWS_CERTIFICATE_PWD ($Password)"
Write-Host "  under Settings -> Secrets and variables -> Actions."
Write-Host ""
Write-Host "IMPORTANT: Keep the .pfx and password private - anyone with both can" -ForegroundColor Red
Write-Host "sign executables that claim to be from '$CertName'." -ForegroundColor Red
Write-Host ""

Read-Host "Press Enter to continue"
