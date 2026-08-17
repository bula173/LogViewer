@echo off
REM Self-Signed Code Signing Certificate Creation Script
REM WARNING: This creates a DEVELOPMENT-ONLY certificate
REM DO NOT use for production/distribution
REM
REM This is a thin wrapper around create_self_signed_cert.ps1. The cert
REM generation itself now uses PowerShell's built-in New-SelfSignedCertificate
REM cmdlet instead of makecert.exe/pvk2pfx.exe, which Microsoft removed from
REM current Windows SDK releases (this .bat previously called them directly
REM and would fail with "'makecert' is not recognized" on a fresh install).

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0create_self_signed_cert.ps1" %*
