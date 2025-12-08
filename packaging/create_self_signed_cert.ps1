# Self-Signed Code Signing Certificate Creation Script
# WARNING: This creates a DEVELOPMENT-ONLY certificate
# DO NOT use for production/distribution

param(
    [string]$CertName = "LogViewerDev",
    [string]$Password = "password123"
)

Write-Host "Creating self-signed code signing certificate..." -ForegroundColor Yellow
Write-Host "WARNING: This certificate is for DEVELOPMENT ONLY" -ForegroundColor Red
Write-Host "It will show security warnings when users run the signed executable" -ForegroundColor Red
Write-Host ""

# Check if makecert and pvk2pfx are available
$makecert = Get-Command makecert -ErrorAction SilentlyContinue
$pvk2pfx = Get-Command pvk2pfx -ErrorAction SilentlyContinue

if (-not $makecert) {
    Write-Host "ERROR: makecert.exe not found. Install Windows SDK." -ForegroundColor Red
    exit 1
}

if (-not $pvk2pfx) {
    Write-Host "ERROR: pvk2pfx.exe not found. Install Windows SDK." -ForegroundColor Red
    exit 1
}

Write-Host "Creating certificate authority..."
& makecert -r -pe -n "CN=$CertName CA" -ss CA -sr CurrentUser `
           -a sha256 -cy authority -sky signature `
           -sv "$CertName CA.pvk" "$CertName CA.cer"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to create certificate authority" -ForegroundColor Red
    exit 1
}

Write-Host "Creating code signing certificate..."
& makecert -pe -n "CN=$CertName" -ss MY -sr CurrentUser `
           -a sha256 -cy end -sky signature `
           -ic "$CertName CA.cer" -iv "$CertName CA.pvk" `
           -sv "$CertName.pvk" "$CertName.cer"

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to create code signing certificate" -ForegroundColor Red
    exit 1
}

Write-Host "Creating PFX file..."
& pvk2pfx -pvk "$CertName.pvk" -spc "$CertName.cer" -pfx "$CertName.pfx" `
          -po $Password

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to create PFX file" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Certificate created successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "Files created:" -ForegroundColor Cyan
Write-Host "  $CertName.pfx    - Certificate file (use with CODE_SIGN_CERTIFICATE)"
Write-Host "  $CertName.cer    - Certificate only"
Write-Host "  $CertName.pvk    - Private key"
Write-Host ""
Write-Host "CMake configuration:" -ForegroundColor Cyan
Write-Host "  -DCODE_SIGN_CERTIFICATE=`"$PWD\$CertName.pfx`""
Write-Host "  -DCODE_SIGN_PASSWORD=`"$Password`""
Write-Host ""
Write-Host "IMPORTANT: Delete these files after development testing" -ForegroundColor Red
Write-Host "Never distribute software signed with self-signed certificates" -ForegroundColor Red
Write-Host ""

Read-Host "Press Enter to continue"