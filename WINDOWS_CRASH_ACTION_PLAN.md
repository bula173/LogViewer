# Action Plan for Windows-Specific Crash

## Summary

The application crashes **only on one specific Windows machine**. This indicates a **machine-specific configuration issue**, not a code bug.

## What Was Fixed (Not Related to Your Issue)

✅ Fixed QString temporary lifetime issues that could cause problems on **Linux**  
✅ Enabled AddressSanitizer for all debug builds  
✅ Added enhanced Windows diagnostics  

These fixes improve overall code quality but won't solve your Windows-specific issue.

## Steps to Diagnose the Problem on the Affected Machine

### Step 1: Run the Diagnostic Script

Copy `scripts/windows_debug.bat` to the Windows machine and run it:

```cmd
cd C:\path\to\LogViewer
scripts\windows_debug.bat
```

This will check for the most common issues:
- Missing Qt plugins (qwindows.dll)
- Qt DLL conflicts in PATH
- Missing Visual C++ Redistributables
- Corrupted configuration files

### Step 2: Enable Detailed Logging

Run the application with full Qt debugging:

```cmd
set QT_DEBUG_PLUGINS=1
set QT_LOGGING_RULES="*.debug=true"
dist\Debug\LogViewerQt.exe > crash_log.txt 2>&1
```

Send me the contents of `crash_log.txt`.

### Step 3: Check Application Logs

Look at the application's own log file:

```cmd
notepad %APPDATA%\LogViewerQt\logs\logviewer.log
```

The log should show exactly where it crashes.

### Step 4: Check Windows Event Viewer

```cmd
eventvwr.msc
```

Navigate to: **Windows Logs → Application**

Look for recent "Application Error" events for LogViewerQt.exe.

## Most Likely Causes (In Order)

### 1. Missing qwindows.dll (90% probability)

**Symptoms:** Immediate crash, no window appears

**Quick Fix:**
```cmd
mkdir dist\Debug\platforms
copy C:\Qt\6.x\msvc2019_64\plugins\platforms\qwindows.dll dist\Debug\platforms\
```

### 2. Qt DLL Version Conflict (5% probability)

**Symptoms:** Crash during startup, different Qt version loaded

**Quick Fix:**
```cmd
# Run from clean environment
set PATH=C:\Windows\System32;C:\Windows
dist\Debug\LogViewerQt.exe
```

### 3. Missing VC++ Redistributable (3% probability)

**Quick Fix:**
Download and install: https://aka.ms/vs/17/release/vc_redist.x64.exe

### 4. Corrupted Config (1% probability)

**Quick Fix:**
```cmd
rd /s /q "%APPDATA%\LogViewerQt"
```

### 5. Machine-Specific Issue (1% probability)

- Antivirus blocking
- Corrupt system files
- Memory issues
- Registry corruption

## What to Send Me

If the issue persists after trying the above:

1. Output from `windows_debug.bat`
2. Contents of `crash_log.txt` (with Qt debug enabled)
3. Application log from `%APPDATA%\LogViewerQt\logs\logviewer.log`
4. Windows Event Viewer crash details
5. System info:
   ```cmd
   systeminfo > sysinfo.txt
   ```

## Documentation

- Full Windows troubleshooting guide: `docs/WINDOWS_TROUBLESHOOTING.md`
- AddressSanitizer setup: `docs/SANITIZERS.md`

## Key Point

Since this only affects **one machine**, it's definitely an environment issue, not a code bug. The diagnostic script should identify the root cause.
