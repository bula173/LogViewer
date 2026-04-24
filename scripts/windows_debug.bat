@echo off
REM Windows Debug Script - Run this on the problematic machine
REM This will help diagnose why the application crashes

echo ========================================
echo LogViewer Windows Diagnostic Tool
echo ========================================
echo.

echo [1/7] Checking Qt installation...
where qt6 >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   OK: Qt found in PATH
    where qt6
) else (
    echo   WARNING: Qt not found in system PATH
)
echo.

echo [2/7] Checking Qt DLLs...
where Qt6Core.dll 2>nul
if %ERRORLEVEL% EQU 0 (
    echo   POTENTIAL ISSUE: Qt6Core.dll found in system PATH
    echo   This may cause version conflicts!
) else (
    echo   OK: No Qt DLLs in system PATH
)
echo.

echo [3/7] Checking Visual C++ Redistributables...
reg query "HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   OK: VC++ 2015-2022 Redistributable found
) else (
    echo   WARNING: VC++ Redistributable may be missing
    echo   Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe
)
echo.

echo [4/7] Checking APPDATA directory...
echo   APPDATA=%APPDATA%
if exist "%APPDATA%\LogViewer" (
    echo   OK: Config directory exists
    dir /b "%APPDATA%\LogViewer"
) else (
    echo   INFO: Config directory not created yet
)
echo.

echo [5/7] Checking application directory...
cd /d "%~dp0.."
echo   App directory: %CD%
if exist "build\windows-debug-qt\src\main\LogViewer.exe" (
    echo   OK: Debug executable found
) else if exist "build\windows-release-qt\src\main\LogViewer.exe" (
    echo   OK: Release executable found
) else (
    echo   ERROR: Executable not found!
    echo   Build first: cmake --preset windows-msys-debug-qt ^&^& cmake --build --preset windows-msys-debug-build-qt
)
echo.

echo [6/7] Checking Qt plugins...
if exist "build\windows-debug-qt\src\main\platforms\qwindows.dll" (
    echo   OK: Qt platform plugin found (Debug)
) else if exist "build\windows-release-qt\src\main\platforms\qwindows.dll" (
    echo   OK: Qt platform plugin found (Release)
) else (
    echo   WARNING: Qt platform plugin (qwindows.dll) not yet deployed
    echo   Run windeployqt after build to copy Qt runtime files.
)
echo.

echo [7/7] Checking DLL dependencies...
where dumpbin >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo   Running dependency check...
    if exist "build\windows-debug-qt\src\main\LogViewer.exe" (
        dumpbin /dependents "build\windows-debug-qt\src\main\LogViewer.exe" | findstr /i "Qt6"
    )
) else (
    echo   INFO: dumpbin not available (install Visual Studio to get it)
)
echo.

echo ========================================
echo Diagnostic Complete
echo ========================================
echo.
echo To run with detailed Qt debug output:
echo   set QT_DEBUG_PLUGINS=1
echo   set QT_LOGGING_RULES="*.debug=true"
echo   build\windows-debug-qt\src\main\LogViewer.exe
echo.
pause
