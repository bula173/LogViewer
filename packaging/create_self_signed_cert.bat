@echo off
REM Self-Signed Code Signing Certificate Creation Script
REM WARNING: This creates a DEVELOPMENT-ONLY certificate
REM DO NOT use for production/distribution

echo Creating self-signed code signing certificate...
echo WARNING: This certificate is for DEVELOPMENT ONLY
echo It will show security warnings when users run the signed executable
echo.

set CERT_NAME=LogViewerDev
set CERT_PASSWORD=password123

echo Creating certificate authority...
makecert -r -pe -n "CN=%CERT_NAME% CA" -ss CA -sr CurrentUser ^
         -a sha256 -cy authority -sky signature ^
         -sv %CERT_NAME%CA.pvk %CERT_NAME%CA.cer

echo.
echo Creating code signing certificate...
makecert -pe -n "CN=%CERT_NAME%" -ss MY -sr CurrentUser ^
         -a sha256 -cy end -sky signature ^
         -ic %CERT_NAME%CA.cer -iv %CERT_NAME%CA.pvk ^
         -sv %CERT_NAME%.pvk %CERT_NAME%.cer

echo.
echo Creating PFX file...
pvk2pfx -pvk %CERT_NAME%.pvk -spc %CERT_NAME%.cer -pfx %CERT_NAME%.pfx ^
        -po %CERT_PASSWORD%

echo.
echo Certificate created successfully!
echo.
echo Files created:
echo   %CERT_NAME%.pfx    - Certificate file (use with CODE_SIGN_CERTIFICATE)
echo   %CERT_NAME%.cer    - Certificate only
echo   %CERT_NAME%.pvk    - Private key
echo.
echo CMake configuration:
echo   -DCODE_SIGN_CERTIFICATE="%CD%\%CERT_NAME%.pfx"
echo   -DCODE_SIGN_PASSWORD="%CERT_PASSWORD%"
echo.
echo IMPORTANT: Delete these files after development testing
echo Never distribute software signed with self-signed certificates
echo.

pause