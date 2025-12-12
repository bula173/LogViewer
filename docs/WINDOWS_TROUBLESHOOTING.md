# Windows Memory/Crash Troubleshooting Guide

## Issue: Application crashes on one specific Windows machine

This document provides steps to diagnose and fix Windows-specific crashes.

## Quick Diagnostic Steps

### 1. Run the Diagnostic Script

```cmd
cd path\to\LogViewer
scripts\windows_debug.bat
```

This will check for:
- Qt installation conflicts
- Missing DLLs
- Missing Visual C++ Redistributables
- Plugin directories
- Configuration issues

### 2. Enable Qt Debug Logging

Run the application with detailed logging:

```cmd
set QT_DEBUG_PLUGINS=1
set QT_LOGGING_RULES="*.debug=true"
set QT_QPA_PLATFORM_PLUGIN_PATH=.\platforms
dist\Debug\LogViewerQt.exe > debug_output.txt 2>&1
```

Check `debug_output.txt` for error messages.

### 3. Check Application Logs

View the application's own log file:
```cmd
notepad %APPDATA%\LogViewerQt\logs\logviewer.log
```

## Common Issues and Solutions

### Issue 1: Missing qwindows.dll (Most Common)

**Symptoms:**
- Application crashes immediately on startup
- Error: "This application failed to start because no Qt platform plugin could be initialized"

**Solution:**
```cmd
# Ensure platforms folder exists next to exe
mkdir dist\Debug\platforms
# Copy qwindows.dll from Qt installation
copy C:\Qt\6.x\msvc2019_64\plugins\platforms\qwindows.dll dist\Debug\platforms\
```

### Issue 2: Qt DLL Version Conflict

**Symptoms:**
- Crash during QApplication creation
- Different Qt version loaded than expected

**Diagnosis:**
```cmd
# Check what's in PATH
echo %PATH% | findstr /i "qt"

# Check loaded DLLs (if you have Process Explorer)
# Look for Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll
```

**Solution:**
1. Remove all Qt paths from system PATH
2. Run from a clean command prompt
3. Or set a clean PATH:
```cmd
set PATH=C:\Windows\System32;C:\Windows
dist\Debug\LogViewerQt.exe
```

### Issue 3: Missing Visual C++ Redistributables

**Symptoms:**
- Error about VCRUNTIME140.dll or MSVCP140.dll
- Application fails to load

**Solution:**
Download and install:
- [Visual C++ Redistributable 2015-2022 (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe)

### Issue 4: Corrupted Configuration File

**Symptoms:**
- Crash after configuration loading
- JSON parse errors in logs

**Solution:**
```cmd
# Delete configuration directory
rd /s /q "%APPDATA%\LogViewerQt"
# Restart application (will create fresh config)
```

### Issue 5: Antivirus/Security Software Interference

**Symptoms:**
- Random crashes
- Memory access violations
- Works on some machines but not others

**Solution:**
1. Temporarily disable antivirus
2. Add exception for LogViewerQt.exe
3. Check Windows Defender logs:
   ```cmd
   powershell Get-MpThreatDetection
   ```

## Machine-Specific Diagnostics

Since the issue affects **only one specific machine**, check:

### 1. System Integrity
```cmd
# Check for corrupt system files
sfc /scannow

# Check for disk errors
chkdsk C: /F
```

### 2. Memory Test
```cmd
# Run Windows Memory Diagnostic
mdsched.exe
```

### 3. Event Viewer
```cmd
# Open Event Viewer
eventvwr.msc
```
Look under:
- Windows Logs → Application (for application crashes)
- Look for "Application Error" events related to LogViewerQt.exe

### 4. Dependency Walker
1. Download Dependency Walker (depends.exe)
2. Open LogViewerQt.exe
3. Look for:
   - Missing DLLs (red highlight)
   - DLL version mismatches
   - Loading order issues

### 5. Process Monitor (Advanced)
1. Download Process Monitor (Sysinternals)
2. Filter: Process Name contains "LogViewer"
3. Capture startup
4. Look for:
   - File not found errors
   - Registry access denied
   - DLL load failures

## Environment Variables to Check

```cmd
echo Application Directory:
cd

echo User APPDATA:
echo %APPDATA%

echo Qt Plugin Path:
echo %QT_PLUGIN_PATH%

echo PATH (check for Qt conflicts):
echo %PATH%
```

## Creating a Minimal Test Case

If the issue persists, create a minimal Qt test:

```cpp
// minimal_test.cpp
#include <QApplication>
#include <QMessageBox>

int main(int argc, char** argv)
{
    try {
        QApplication app(argc, argv);
        QMessageBox::information(nullptr, "Test", "Qt is working!");
        return 0;
    } catch (...) {
        return 1;
    }
}
```

Build and test:
```cmd
qmake -project
qmake
nmake
minimal_test.exe
```

If this fails, the issue is with Qt installation on that machine.

## Reporting the Issue

If none of the above helps, collect:

1. Output from `windows_debug.bat`
2. Content of `debug_output.txt` (with Qt debug enabled)
3. Application log file from `%APPDATA%\LogViewerQt\logs\`
4. Windows Event Viewer crash details
5. System information:
   ```cmd
   systeminfo > sysinfo.txt
   ```

## AddressSanitizer on Windows (MSYS2/MinGW)

Note: ASAN is **not supported on Windows with MSVC**. For memory debugging on Windows:

1. Use Windows Debug build with debug heap:
   ```cmd
   set _NO_DEBUG_HEAP=0
   dist\Debug\LogViewerQt.exe
   ```

2. Or use Application Verifier (Microsoft tool)

3. Or test on Linux with ASAN enabled (build with `linux-debug-qt` preset)

## Contact

If you need help, provide:
- Windows version
- Qt version
- Output from diagnostic script
- Application logs
- Any error messages
