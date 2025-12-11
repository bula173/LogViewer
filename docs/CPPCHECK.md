# Static Analysis with Cppcheck

## Overview

Cppcheck is integrated into the build system for static code analysis. It checks for:
- Potential bugs
- Undefined behavior
- Dangerous coding constructs
- Performance issues
- Portability issues
- Style violations

## Quick Start

### Using Build Presets (Recommended)

```bash
# Run cppcheck analysis
cmake --build --preset macos-cppcheck        # macOS
cmake --build --preset linux-cppcheck        # Linux

# Run clang-tidy analysis
cmake --build --preset macos-clang-tidy      # macOS
cmake --build --preset linux-clang-tidy      # Linux

# Format code
cmake --build --preset macos-format          # macOS
cmake --build --preset linux-format          # Linux

# Run all quality tools (format + cppcheck)
cmake --build --preset macos-quality-tools   # macOS
cmake --build --preset linux-quality-tools   # Linux

# View the report
cat build/macos-debug-qt/cppcheck-report.txt
```

### Using Build Targets (Alternative)

```bash
# Run cppcheck
cmake --build build/macos-debug-qt --target cppcheck

# View the report
cat build/macos-debug-qt/cppcheck-report.txt
```

### Enable During Compilation

Cppcheck is **automatically enabled** for debug builds by default. To disable:

```bash
cmake --preset macos-debug-qt -DENABLE_CPPCHECK=OFF
cmake --build build/macos-debug-qt
```

## Available Targets

### `cppcheck`
Run complete cppcheck analysis on all source files:
```bash
cmake --build build/macos-debug-qt --target cppcheck
```

Output is saved to: `build/macos-debug-qt/cppcheck-report.txt`

### `format`
Run clang-format to auto-format all code:
```bash
cmake --build build/macos-debug-qt --target format
```

### `tidy`
Run clang-tidy for additional static analysis:
```bash
cmake --build build/macos-debug-qt --target tidy
```

### `run_tools`
Run both format and cppcheck:
```bash
cmake --build build/macos-debug-qt --target run_tools
```

## Integration

### Compile-Time Checks (Default for Debug)

When `ENABLE_CPPCHECK=ON` (default for debug presets), cppcheck runs on every file during compilation:

```cmake
# Already enabled in these presets:
- macos-debug-qt
- macos-debug-wx  
- linux-debug-qt
- linux-debug-wx
```

Warnings appear in build output:
```
[5/70] Building CXX object src/application/db/EventsContainer.cpp.o
EventsContainer.cpp:58:20: warning: Consider using std::transform algorithm instead of a raw loop. [useStlAlgorithm]
```

### Standalone Analysis

The standalone `cppcheck` target provides a comprehensive report:

```bash
cmake --build build/macos-debug-qt --target cppcheck
```

This is useful for:
- CI/CD pipelines
- Pre-commit checks
- Generating reports

## Configuration

### Current Settings

The cppcheck configuration in `cmake/cleanCppExtensions.cmake`:

```cmake
--enable=warning,performance,portability,style
--inline-suppr           # Allow inline suppressions
--std=c++20             # C++20 standard
--template=gcc          # GCC-style output
-I src/                 # Include directories
```

### Suppressing Warnings

#### Inline Suppression
```cpp
// cppcheck-suppress useStlAlgorithm
for (const auto& item : data) {
    process(item);
}
```

#### File-Level Suppression
Already configured to suppress:
- `unusedFunction` - Functions only used in tests
- `missingIncludeSystem` - System headers
- `unmatchedSuppression` - Unmatched suppressions

### Adding More Checks

Edit `cmake/cleanCppExtensions.cmake` to enable additional checks:

```cmake
--enable=all           # Enable all checks (can be noisy)
--check-level=exhaustive  # More thorough but slower
```

## CI/CD Integration

### GitHub Actions Example

```yaml
- name: Run Cppcheck
  run: |
    cmake --preset linux-debug-qt
    cmake --build build/linux-debug-qt --target cppcheck
    
- name: Upload Report
  uses: actions/upload-artifact@v3
  with:
    name: cppcheck-report
    path: build/linux-debug-qt/cppcheck-report.txt
```

### Fail on Warnings

To fail the build on cppcheck warnings:

```bash
cmake --build build/macos-debug-qt --target cppcheck 2>&1 | \
  tee cppcheck.log && ! grep -E "warning|error" cppcheck.log
```

## Common Warnings

### `useStlAlgorithm`
**Issue:** Loop can be replaced with STL algorithm  
**Example:**
```cpp
// Warning
for (const auto& item : items) {
    result.push_back(transform(item));
}

// Better
std::transform(items.begin(), items.end(), 
               std::back_inserter(result), transform);
```

### `normalCheckLevelMaxBranches`
**Issue:** Too many branches, analysis limited  
**Solution:** Use `--check-level=exhaustive` or simplify function

### `unknownMacro`
**Issue:** Macro not recognized (common with GUI frameworks)  
**Solution:** Add to suppression list or configure macro

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| macOS | ✅ | Install: `brew install cppcheck` |
| Linux | ✅ | Install: `apt install cppcheck` |
| Windows | ✅ | Install via MSYS2 or download from cppcheck.net |

## Performance

- **Compile-time checks**: Adds ~5-10% to build time
- **Standalone analysis**: ~30 seconds for full codebase
- **Parallel execution**: Automatic

## Troubleshooting

### "cppcheck not found"
```bash
# macOS
brew install cppcheck

# Ubuntu/Debian
sudo apt-get install cppcheck

# Arch
sudo pacman -S cppcheck
```

### Too Many False Positives

Disable compile-time checks:
```bash
cmake --preset macos-debug-qt -DENABLE_CPPCHECK=OFF
```

Use standalone target for cleaner reports.

### Report Not Generated

Check build directory:
```bash
ls -la build/macos-debug-qt/cppcheck-report.txt
```

## Additional Tools

### Clang-Tidy

For more advanced analysis:
```bash
cmake --preset macos-debug-qt -DENABLE_CLANG_TIDY=ON
cmake --build build/macos-debug-qt
```

### Combine All Tools

```bash
# Format, analyze, and build
cmake --build build/macos-debug-qt --target run_tools
cmake --build build/macos-debug-qt
```

## References

- [Cppcheck Manual](http://cppcheck.net/manual.pdf)
- [Cppcheck GitHub](https://github.com/danmar/cppcheck)
- Project config: `cmake/cleanCppExtensions.cmake`
