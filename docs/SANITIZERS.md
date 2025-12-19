# AddressSanitizer Setup

## Overview

AddressSanitizer (ASAN) is automatically enabled for all **Debug** builds to catch memory errors:
- Use-after-free
- Heap buffer overflow
- Stack buffer overflow
- Memory leaks (Linux only)
- Use-after-return
- Double-free

## Automatic Configuration

ASAN is enabled by default in the following presets:
- `macos-debug-wx`
- `macos-debug-qt`
- `linux-debug-wx`
- `linux-debug-qt`

## Building with ASAN

### macOS
```bash
# Configure
cmake --preset macos-debug-qt

# Build
cmake --build build/macos-debug-qt

# Run (ASAN reports appear in terminal)
./dist/Debug/LogViewer.app/Contents/MacOS/LogViewer
```

### Linux
```bash
# Configure
cmake --preset linux-debug-qt

# Build
cmake --build build/linux-debug-qt

# Run with options
ASAN_OPTIONS=halt_on_error=0:log_path=asan.log ./build/linux-debug-qt/src/LogViewer
```

## ASAN Runtime Options

Set environment variables before running:

```bash
# Continue on error, log to file
export ASAN_OPTIONS=halt_on_error=0:log_path=asan_log.txt

# Strict mode - halt on first error
export ASAN_OPTIONS=halt_on_error=1

# Detect stack-use-after-return (slower)
export ASAN_OPTIONS=detect_stack_use_after_return=1
```

## Common Options

| Option | Description | Default |
|--------|-------------|---------|
| `halt_on_error` | Stop on first error | 1 |
| `log_path` | Write reports to file | stderr |
| `detect_leaks` | Check for memory leaks (Linux) | 1 |
| `detect_stack_use_after_return` | Detect stack UAF | 0 |

## Reading ASAN Output

When ASAN detects an error, it prints:
1. **Error type**: e.g., "heap-use-after-free"
2. **Stack trace**: Where the error occurred
3. **Allocation trace**: Where the memory was allocated
4. **Deallocation trace**: Where it was freed

Example:
```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x603000000010
READ of size 4 at 0x603000000010 thread T0
    #0 0x4c6b8f in MainWindow::UpdateStatusText() MainWindow.cpp:489
    #1 0x4c7a2e in MainWindow::HandleDroppedFile() MainWindow.cpp:620
```

## Disabling ASAN

To build debug without ASAN:

```bash
cmake --preset macos-debug-qt -DLOGVIEWER_SANITIZER=None
cmake --build build/macos-debug-qt
```

## Other Sanitizers

Available sanitizers (set via `-DLOGVIEWER_SANITIZER=<name>`):
- `None` - Disabled
- `Address` - Memory errors (default for Debug)
- `Thread` - Data races
- `Undefined` - Undefined behavior
- `Memory` - Uninitialized reads (Linux only)

**Note**: Only one sanitizer can be active at a time.

## Performance Impact

ASAN adds overhead:
- Memory: ~2-3x increase
- Runtime: ~2x slower
- Not suitable for benchmarking

## Platform Support

| Platform | ASAN | Leak Detection |
|----------|------|----------------|
| macOS | ✅ | ❌ |
| Linux | ✅ | ✅ |
| Windows (MSYS2) | ❌ | ❌ |

## Troubleshooting

### False Positives
If you see false positives from Qt libraries, create a suppression file:

```bash
# Create suppressions.txt
cat > asan_suppressions.txt << EOF
leak:libQt6Core
leak:libQt6Gui
EOF

# Use it
export ASAN_OPTIONS=suppressions=asan_suppressions.txt
```

### Slow Startup
ASAN initialization can be slow. This is normal.

### MinGW/Windows
AddressSanitizer is not supported on MinGW. Use Linux or macOS for memory debugging.
