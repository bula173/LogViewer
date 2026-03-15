# Build System Guide and Improvements

This document outlines the LogViewer build system configuration, available presets, and optimization strategies.

## Overview

LogViewer uses **CMake 3.25+** with platform-specific presets for streamlined cross-platform building. The build system supports:
- **macOS** (Apple Silicon & Intel)
- **Windows** (MSYS2, MinGW)
- **Linux** (GCC, Clang)

**C++ Standard**: C++20 (required)

---

## Quick Start

### macOS
```bash
# Debug build
cmake --build --preset macos-debug-build-qt

# Release build
cmake --build --preset macos-release-build-qt

# Run tests
./build/macos-debug-qt/tests/bin/LogViewer_tests
```

### Linux
```bash
# Debug build
cmake --build --preset linux-debug-build-qt

# Run application
./build/linux-debug-qt/src/main/LogViewer

# Run tests
./build/linux-debug-qt/tests/bin/LogViewer_tests
```

### Windows (MSYS2)
```cmd
cmake --build --preset windows-msys-debug-qt
.\build\windows-msys-debug-qt\src\main\LogViewer.exe
```

---

## Build Presets

### Available Presets

**macOS**:
- `macos-debug-build-qt` - Debug build with Qt
- `macos-release-build-qt` - Release build with Qt
- `macos-debug-test` - Run unit tests (macOS debug)

**Linux**:
- `linux-debug-build-qt` - Debug build
- `linux-release-build-qt` - Release build
- `linux-debug-test` - Run tests

**Windows (MSYS2)**:
- `windows-msys-debug-qt` - Debug build
- `windows-msys-release-qt` - Release build

### Using Presets

```bash
# List all available presets
cmake --list-presets

# Configure with preset (optional, auto-called with --build)
cmake --preset <preset-name>

# Build
cmake --build --preset <preset-name>

# Build specific target
cmake --build --preset <preset-name> --target LogViewer_tests

# Clean build
cmake --build --preset <preset-name> --clean-first

# Parallel build (use all CPU cores)
cmake --build --preset <preset-name> -j$(nproc)
```

---

## Build Directory Structure

All presets output to `build/<preset-name>/`:

```
build/
├── macos-debug-qt/
│   ├── bin/
│   │   ├── LogViewer.app/
│   │   └── tests/
│   ├── lib/
│   ├── plugins/
│   └── CMakeFiles/
└── linux-debug-qt/
    ├── src/main/
    ├── lib/
    ├── plugins/
    └── tests/bin/
```

**Key Output Paths**:
- Application: `build/<preset>/src/main/LogViewer`
- Tests: `build/<preset>/tests/bin/LogViewer_tests`
- Libraries: `build/<preset>/lib/`
- Plugins: `build/<preset>/plugins/`

---

## Build Configuration

### C++ Standard
LogViewer requires **C++20**. This is enforced in CMakeLists.txt:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### Compiler Requirements

| Platform | Compiler | Min Version |
|----------|----------|------------|
| macOS | Clang/Apple Clang | 15.0 |
| Linux | GCC | 11.0 |
| Linux | Clang | 15.0 |
| Windows | MSVC | 2019 |
| Windows | MinGW/GCC | 11.0 |

### Qt Version
- **Minimum**: Qt 6.5
- **Recommended**: Qt 6.7+

**Installation**:
```bash
# macOS
brew install qt

# Linux (Ubuntu/Debian)
sudo apt-get install qt6-base-dev qt6-tools-dev

# Windows (via vcpkg or Qt installer)
vcpkg install qt6:x64-windows
```

---

## Build Optimization

### Release Build Flags
Release presets automatically enable:
- `-O3` optimization
- Link-time optimization (LTO)
- Symbol stripping

```bash
cmake --build --preset <preset>-release-qt -j$(nproc)
```

### Debug Build with Full Symbols
Debug presets include:
- `-g` full debugging symbols
- `-O0` no optimization (faster compilation)
- Assertions enabled

```bash
cmake --build --preset <preset>-debug-qt
```

### Parallel Compilation
Use all CPU cores to speed up builds:

```bash
# macOS/Linux
cmake --build --preset <preset> -j$(nproc)

# Windows
cmake --build --preset <preset> -j%NUMBER_OF_PROCESSORS%
```

### Incremental Builds
Only modified files are recompiled:

```bash
cmake --build --preset <preset>
```

### Clean Build
Remove all generated files and rebuild:

```bash
cmake --build --preset <preset> --clean-first
```

---

## Dependency Management

### Third-Party Dependencies

Located in `thirdparty/` and managed by FetchContent:

- **nlohmann/json** - JSON serialization
- **spdlog** - Logging framework
- **fmt** - String formatting
- **CURL** - HTTP requests
- **Google Test** - Unit testing
- **Expat** - XML parsing
- **libarchive** - Archive handling

Dependencies are automatically downloaded and built during CMake configuration.

### Reducing Build Time

**Option 1: Use Pre-built Dependencies**
```bash
# Pass pre-built paths to CMake
cmake --preset <preset> \
  -DCMAKE_PREFIX_PATH=/path/to/qt;/path/to/dependencies
```

**Option 2: Ccache Integration**
```bash
# Enable compiler caching (speeds up rebuilds)
brew install ccache  # macOS

# Configure CMake to use ccache
export CC="ccache gcc"
export CXX="ccache g++"
cmake --build --preset <preset>
```

---

## Troubleshooting Build Issues

### Qt Not Found
```bash
# Manually specify Qt path
cmake --preset <preset> -DQt6_DIR=/opt/homebrew/opt/qt/lib/cmake/Qt6
```

### Out of Memory During Linking
```bash
# Reduce parallel jobs
cmake --build --preset <preset> -j2
```

### C++20 Features Not Recognized
```bash
# Verify compiler version
gcc --version    # Should be 11+
clang --version  # Should be 15+
```

### Pre-compiled Header (PCH) Issues
```bash
# Disable PCH if causing problems
cmake --preset <preset> -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
```

---

## Development Workflow

### Setup Development Environment

```bash
# 1. Clone repository
git clone https://github.com/bula173/LogViewer.git
cd LogViewer

# 2. Create build directory
cmake --preset macos-debug-build-qt

# 3. Build
cmake --build --preset macos-debug-build-qt -j$(nproc)

# 4. Run tests
./build/macos-debug-qt/tests/bin/LogViewer_tests

# 5. Run application
./build/macos-debug-qt/src/main/LogViewer
```

### Iterative Development

After making code changes:

```bash
# Rebuild incrementally (only changed files)
cmake --build --preset <preset>

# Run affected tests
./build/<preset>/tests/bin/LogViewer_tests --gtest_filter=<TestName>

# Run application
./build/<preset>/src/main/LogViewer
```

### Code Quality Checks

```bash
# Static analysis with cppcheck
cppcheck --enable=all src/

# Clang-tidy checks (if configured)
cmake --build --preset <preset> -- -j$(nproc)

# Format with clang-format
find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

---

## Continuous Integration

### GitHub Actions
Build is validated on every commit via GitHub Actions. See `.github/workflows/` for CI configuration.

**CI Runs**:
- macOS debug and release builds
- Linux debug and release builds
- Windows MSYS2 builds
- All 45+ unit tests
- Static analysis (cppcheck)

### Local Pre-commit Validation

Before committing, verify:

```bash
# 1. Build succeeds
cmake --build --preset macos-debug-build-qt

# 2. Tests pass
./build/macos-debug-qt/tests/bin/LogViewer_tests

# 3. No clang-format violations
find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format --dry-run -Werror

# 4. Static analysis passes
cppcheck --suppress=missingIncludeSystem src/
```

---

## Advanced Build Options

### Enable All Warnings
```bash
cmake --preset <preset> -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic"
```

### Force Color Output
```bash
cmake --build --preset <preset> -DFORCE_COLORED_OUTPUT=ON
```

### Build Verbose (Show Full Commands)
```bash
cmake --build --preset <preset> --verbose
```

### Generate Compilation Database
```bash
cmake --preset <preset> -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build --preset <preset>
# Generates: compile_commands.json (for IDE integration)
```

---

## Performance Benchmarks

Typical build times (clean build, parallel with `-j$(nproc)`):

| Platform | Config | Time |
|----------|--------|------|
| macOS M2 | Debug | ~45s |
| macOS M2 | Release | ~90s |
| Linux | Debug | ~40s |
| Linux | Release | ~85s |
| Windows | Debug | ~60s |

**With ccache after first build**: ~10-15s (incremental)

---

## Distribution and Packaging

### macOS App Bundle
```bash
cmake --build --preset macos-release-build-qt
# Creates: build/macos-release-qt/src/main/LogViewer.app
# Fully self-contained, ready to distribute
```

### Linux Package
```bash
cmake --build --preset linux-release-build-qt
# Creates: build/linux-release-qt/src/main/LogViewer
```

### Windows Installer
```cmd
cmake --build --preset windows-msys-release-qt
REM Creates executable and dependencies for packaging
```

---

## See Also

- `DEVELOPMENT.md` - Development guidelines and workflows
- `BUILDING_DOCUMENTATION.md` - Doxygen documentation generation
- `.github/workflows/` - CI/CD configuration
