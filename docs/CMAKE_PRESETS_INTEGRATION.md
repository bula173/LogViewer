# CMake Presets Integration

This document describes the integration of CMake Presets with VSCode configuration files.

## Overview

The project now uses **CMake Presets** (CMakePresets.json) as the single source of truth for build configuration. All VSCode tasks and launch configurations reference these presets, eliminating duplication and ensuring consistency.

## Benefits

✅ **Single Source of Truth** - All build settings in one place (CMakePresets.json)  
✅ **No Duplication** - Compiler paths, flags, and options defined once  
✅ **Cross-Platform** - Automatically adapts to Windows/Linux/macOS  
✅ **Easier Maintenance** - Changes only need to be made in CMakePresets.json  
✅ **Modern CMake** - Uses CMake 3.25+ workflow presets for complete build pipelines  
✅ **IDE Integration** - Works seamlessly with VSCode CMake Tools extension

## CMake Presets Structure

### Configure Presets

Each platform has debug and release configure presets:

- **Linux**: `linux-debug`, `linux-release`
- **macOS**: `macos-debug`, `macos-release`
- **Windows**: `windows-msys-debug`, `windows-msys-release`

These define:
- Build directories (e.g., `build/windows-debug`)
- Generator (Ninja)
- Build type (Debug/Release)
- Compiler settings (Clang for Windows)
- Platform-specific paths

### Build Presets

Build presets reference configure presets:

- `linux-debug-build` → `linux-debug`
- `macos-release-build` → `macos-release`
- `windows-msys-debug-build` → `windows-msys-debug`
- etc.

### Test Presets

Test presets enable running CTest with proper configuration:

- `linux-debug-test`, `linux-release-test`
- `macos-debug-test`, `macos-release-test`
- `windows-msys-debug-test`, `windows-msys-release-test`

Output shown on failure for all test configurations.

### Package Presets

Platform-specific packaging:

- **Linux**: DEB, TBZ2, TXZ packages
- **macOS**: DragNDrop (DMG)
- **Windows**: NSIS installer

### Workflow Presets

Complete build pipelines combining configure → build → test → package:

- `linux-debug-workflow`, `linux-release-workflow`
- `macos-debug-workflow`, `macos-release-workflow`
- `windows-msys-debug-workflow`, `windows-msys-release-workflow`

## VSCode Tasks Integration

### Updated Tasks (.vscode/tasks.json)

All tasks now use CMake presets:

#### Configure Tasks
```json
{
    "label": "Configure Debug",
    "type": "shell",
    "windows": {
        "command": "cmake",
        "args": ["--preset", "windows-msys-debug"]
    }
}
```

#### Build Tasks
```json
{
    "label": "Build Debug",
    "type": "shell",
    "windows": {
        "command": "cmake",
        "args": ["--build", "--preset", "windows-msys-debug-build"]
    }
}
```

#### Test Tasks
```json
{
    "label": "Test Debug",
    "type": "shell",
    "windows": {
        "command": "ctest",
        "args": ["--preset", "windows-msys-debug-test"]
    }
}
```

#### Workflow Tasks
```json
{
    "label": "Full Debug Workflow",
    "windows": {
        "command": "cmake",
        "args": ["--workflow", "--preset", "windows-msys-debug-workflow"]
    }
}
```

### Available Tasks

| Task | Description |
|------|-------------|
| **Configure Debug** | Configure debug build using preset |
| **Configure Release** | Configure release build using preset |
| **Build Debug** | Build debug configuration (default) |
| **Build Release** | Build release configuration |
| **Test Debug** | Run debug tests with CTest |
| **Test Release** | Run release tests with CTest |
| **Run App (Debug)** | Launch debug application |
| **Run Tests (Debug)** | Execute tests with output |
| **Package Release** | Create platform-specific package |
| **Full Debug Workflow** | Complete debug pipeline |
| **Full Release Workflow** | Complete release pipeline with packaging |
| **Clean** | Remove all build artifacts |
| **Static Analysis** | Run cppcheck |

## VSCode Launch Configurations

### Updated Configurations (.vscode/launch.json)

All launch configurations updated to:
1. Reference correct build directories from presets
2. Use updated task names (e.g., "Build Debug" instead of "Build debug with CMake")
3. Add platform-specific debugger paths for Windows
4. Fix macOS bundle paths

### Available Launch Configurations

| Configuration | Description |
|---------------|-------------|
| **Launch Debug** | Debug the application with GDB/LLDB |
| **Launch Release** | Run release build with debugger |
| **Run (Debug Build)** | Run debug build without stopping |
| **Launch Tests (All)** | Debug all unit tests |
| **Launch Tests (Verbose)** | Debug tests with timing info |
| **Launch Single Test** | Debug specific test with filter |
| **Launch with File Argument** | Debug app with log file argument |

### Test Paths Updated

Test executables now located in preset build directories:

- Linux: `build/linux-debug/tests/LogViewer_tests`
- macOS: `build/macos-debug/tests/LogViewer_tests`
- Windows: `build/windows-debug/tests/LogViewer_tests.exe`

## Build Directory Structure

```
build/
├── linux-debug/           # Linux debug builds
├── linux-release/         # Linux release builds
├── macos-debug/           # macOS debug builds
├── macos-release/         # macOS release builds
├── windows-debug/         # Windows MSYS2 debug builds
└── windows-release/       # Windows MSYS2 release builds
```

Each build directory contains:
- CMake cache and configuration
- Compiled objects
- Executables (in `src/`)
- Test executables (in `tests/`)
- Generated files

## Command-Line Usage

### Using Presets Directly

```bash
# Configure
cmake --preset windows-msys-debug

# Build
cmake --build --preset windows-msys-debug-build

# Test
ctest --preset windows-msys-debug-test

# Package
cpack --preset windows-msys-release-package

# Complete workflow
cmake --workflow --preset windows-msys-release-workflow
```

### Legacy Commands (Still Work)

```bash
# Manual configuration (not recommended)
cmake -S . -B build/windows-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Manual build
cmake --build build/windows-debug
```

## Platform-Specific Notes

### Windows (MSYS2)

- Uses Clang compiler from MSYS2
- Requires MSYS2 MinGW64 environment
- GDB debugger path: `C:/msys64/mingw64/bin/gdb.exe`
- Build directories: `build/windows-debug`, `build/windows-release`

### Linux

- Uses system GCC/Clang
- Standard paths
- Build directories: `build/linux-debug`, `build/linux-release`

### macOS

- Uses Xcode's Clang (via Command Line Tools)
- LLDB debugger
- App bundle handling for `.app` packages
- Build directories: `build/macos-debug`, `build/macos-release`

## Migration Guide

If you have existing build directories, you may need to:

1. **Clean old builds**:
   ```bash
   rm -rf build/debug build/release
   ```

2. **Reconfigure with presets**:
   ```bash
   cmake --preset windows-msys-debug
   ```

3. **Build with preset**:
   ```bash
   cmake --build --preset windows-msys-debug-build
   ```

## Compatibility

### Backward Compatibility

Legacy task names are maintained as aliases:
- "Build debug with CMake" → calls "Build Debug"
- "Build release with CMake" → calls "Build Release"

### CMake Version Requirements

- **Minimum**: CMake 3.25 (for workflow presets)
- **Recommended**: CMake 3.27+

### VSCode Extensions

Works with:
- **CMake Tools** (ms-vscode.cmake-tools) - For preset selection in UI
- **C/C++** (ms-vscode.cpptools) - For debugging
- **C/C++ Extension Pack** - Complete C++ development

## Troubleshooting

### "Preset not found" errors

Ensure you're using CMake 3.25+:
```bash
cmake --version
```

### Wrong build directory used

VSCode might cache old paths. Reload window:
- Press `Ctrl+Shift+P` → "Developer: Reload Window"

### Compiler not found (Windows)

Ensure MSYS2 is installed and paths are correct in CMakePresets.json:
```json
"CMAKE_C_COMPILER": "C:/msys64/mingw64/bin/clang.exe",
"CMAKE_CXX_COMPILER": "C:/msys64/mingw64/bin/clang++.exe"
```

### Debugger fails to start

Check debugger path in launch.json:
```json
"miDebuggerPath": "C:/msys64/mingw64/bin/gdb.exe"
```

## Best Practices

1. **Always use presets** for consistent builds
2. **Use workflow presets** for complete build/test/package cycles
3. **Let CMakePresets.json** be the source of truth
4. **Don't modify build directories manually** - they're managed by CMake
5. **Use VSCode tasks** instead of manual terminal commands
6. **Configure once, build many times** - reconfigure only when changing settings

## Future Enhancements

Possible improvements:
- [ ] Add sanitizer presets (ASan, TSan, UBSan)
- [ ] Add coverage presets for code coverage analysis
- [ ] Add cross-compilation presets
- [ ] Add Docker build presets
- [ ] Add CI/CD workflow presets

## References

- [CMake Presets Documentation](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [VSCode C++ Configuration](https://code.visualstudio.com/docs/cpp/config-msvc)
- [CMake Workflows](https://cmake.org/cmake/help/latest/manual/cmake.1.html#workflow-mode)

---

**Last Updated**: 2025-11-13  
**CMake Version**: 3.25+  
**VSCode Version**: 1.80+
