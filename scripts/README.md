# LogViewer Scripts

Helper scripts for building, running, and testing LogViewer locally.

## Scripts

| Script | Platform | Purpose |
|--------|----------|---------|
| `configure_and_build_osx.sh` | macOS | Configure + build via CMake presets |
| `run_debug.sh` | macOS | Launch the debug build |
| `run-gh-actions.sh` | macOS / Linux | Simulate CI workflows locally |
| `windows_debug.bat` | Windows | Diagnose Windows startup issues |
| `generate_ertms_logs.py` | Any | Generate ERTMS test log data |

---

## configure_and_build_osx.sh

Configure and build on macOS using CMake presets.

```bash
./scripts/configure_and_build_osx.sh          # debug (default)
./scripts/configure_and_build_osx.sh release
./scripts/configure_and_build_osx.sh asan
```

## run_debug.sh

Launch the macOS debug binary directly.

```bash
./scripts/run_debug.sh
```

Build first if the binary is missing:
```bash
cmake --build --preset macos-debug-build-qt
```

## run-gh-actions.sh

Simulate GitHub Actions workflows locally.

- **macOS**: runs the equivalent cmake presets directly (no Docker).
- **Linux**: delegates to [`act`](https://github.com/nektos/act) with Docker.

```bash
./scripts/run-gh-actions.sh [workflow] [--dry-run]
```

### Workflows

| Workflow | What it does |
|----------|-------------|
| `build` (default) | Configure + build debug and release |
| `quality` | clang-tidy, cppcheck, format check |
| `asan` | Build + test with AddressSanitizer |
| `tsan` | Build + test with ThreadSanitizer |
| `test` | CTest on the debug build |
| `all` | All of the above in sequence |

```bash
./scripts/run-gh-actions.sh build
./scripts/run-gh-actions.sh quality
./scripts/run-gh-actions.sh asan
./scripts/run-gh-actions.sh all --dry-run   # preview commands without running
./scripts/run-gh-actions.sh --list          # list available workflows
```

### Prerequisites

**macOS** – cmake + Qt 6 in PATH (same as a normal build, no extra tools).

**Linux (act)**:
```bash
# Install act
brew install act                 # or
curl -s https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash

# Docker must be running
```

act uses the `catthehacker/ubuntu:act-22.04` image which closely matches the
GitHub-hosted `ubuntu-latest` runner.

## windows_debug.bat

Diagnostic tool for Windows: checks Qt DLLs, VC++ runtimes, config directory,
executable presence, and DLL dependencies.

```cmd
scripts\windows_debug.bat
```

After a successful cmake build the executable is at:
```
build\windows-debug-qt\src\main\LogViewer.exe
build\windows-release-qt\src\main\LogViewer.exe
```

## generate_ertms_logs.py

Appends ~1000 synthetic ERTMS events to `test_data/ertms_logs.xml`.

```bash
python3 scripts/generate_ertms_logs.py
```

The path is resolved relative to the script location — no hardcoded paths.

---

## CMake Preset Quick Reference

```bash
# Debug
cmake --preset macos-debug-qt
cmake --build --preset macos-debug-build-qt
ctest --preset macos-debug-test-qt

# Release
cmake --preset macos-release-qt
cmake --build --preset macos-release-build-qt

# ASan
cmake --preset macos-asan-qt
cmake --build --preset macos-asan-build-qt
ctest --preset macos-asan-test-qt

# Quality (clang-tidy + cppcheck)
cmake --preset macos-checks-qt
cmake --build --preset macos-checks-build-qt

# Format
cmake --build --preset macos-format-check   # read-only check
cmake --build --preset macos-format-apply   # auto-fix
```

Replace `macos` with `linux` or `windows-msys` for other platforms.
