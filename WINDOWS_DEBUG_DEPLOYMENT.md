# Windows Debug Build DLL Deployment Fix

## Problem
Error `0xc0000602` (STATUS_DLL_INIT_FAILED) when running Debug builds on Windows.

This error occurs because:
1. Qt DLLs were not being deployed to `dist/Debug` 
2. Only `dist/Release` was being populated
3. The Debug executable couldn't find required Qt DLLs

## Solution
Updated `src/main/CMakeLists.txt` to use `DIST_BUILD` variable that changes based on `CMAKE_BUILD_TYPE`:
- Debug builds → `dist/Debug`
- Release builds → `dist/Release`

### Changes Made

1. **Added DIST_BUILD variable** (line ~27):
```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DIST_BUILD "${DIST_ROOT}/Debug")
else()
    set(DIST_BUILD "${DIST_ROOT}/Release")
endif()
```

2. **Updated windeployqt to deploy correct DLLs** (line ~110):
- Uses `--debug` flag for Debug builds
- Uses `--release` flag for Release builds
- Deploys to separate `qt_deploy_${CMAKE_BUILD_TYPE}` directories
- Copies DLLs to `${DIST_BUILD}` instead of hardcoded `${DIST_RELEASE}`

3. **Updated MinGW DLL copying**:
- Now copies runtime DLLs to `${DIST_BUILD}` based on build type

## How to Test

### Rebuild Debug Build
```bash
cd /Users/Marcin/workspace/cpp/LogViewer
cmake --build build/windows-debug-qt --config Debug
```

### Verify Deployment
Check that `dist/Debug` contains:
```
dist/Debug/
├── LogViewerQt.exe
├── Qt6Core.dll (Debug version - should be larger)
├── Qt6Cored.dll (Debug version with 'd' suffix)
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── platforms/
│   └── qwindowsd.dll (Debug version)
└── etc/
    └── config.json
```

**Debug DLLs are larger** than Release DLLs (they contain debugging symbols).

### Run Debug Build
```bash
cd dist/Debug
./LogViewerQt.exe
```

## Diagnostics

The application has built-in diagnostics in `src/main/MyAppQt.cpp::CheckQtLibraries()` that will:
1. Check Qt version
2. Verify `platforms/` directory exists
3. List available platform plugins
4. Check for Qt DLL conflicts in system PATH

If the crash persists, check the log file in:
- Windows: `%APPDATA%/LogViewerQt/logs/LogViewerQt.log`

## Common Issues

### 1. Missing platforms/qwindowsd.dll
```
CRITICAL: platforms directory not found
```
**Solution**: Rebuild to trigger windeployqt

### 2. Wrong DLL Version (Release DLL with Debug EXE)
```
The application was unable to start correctly (0xc0000602)
```
**Solution**: Delete `dist/Debug` and rebuild

### 3. Multiple Qt Versions in PATH
```
Qt directories found in system PATH - this may cause DLL conflicts
```
**Solution**: Run from clean PATH or use:
```batch
set PATH=C:\Windows\System32;C:\Windows
dist\Debug\LogViewerQt.exe
```

### 4. Missing MSYS2 Runtime DLLs
Check that these are present in `dist/Debug`:
- `libgcc_s_seh-1.dll`
- `libstdc++-6.dll`
- `libwinpthread-1.dll`

## CI/CD Note
The GitHub Windows workflow already builds both Debug and Release.
With this fix, both will create proper installers:
- `windows-Debug-qt-installer` - includes Debug DLLs
- `windows-Release-qt-installer` - includes Release DLLs
