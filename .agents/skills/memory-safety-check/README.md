# Memory & Resource Safety Check Skill

This skill provides a comprehensive workflow for detecting and fixing memory safety issues in C++ projects using compiler-based sanitizers.

## Quick Start

**Invoke the skill by typing:**
```
/memory safety check
```

Or ask the agent directly:
```
Check memory safety in this code
Run ASan on the tests
Verify there are no memory leaks
```

## What's Included

### Core Skill: SKILL.md
A 12-step procedure for building, running, and analyzing sanitizer tests. Covers:
- Building with ASan, MSan, UBSan
- Running tests under sanitizers
- Parsing and analyzing reports
- Reproducing issues in isolation
- Implementing fixes
- Validating fixes
- Extended testing with multiple sanitizers
- Documentation and prevention

### Reference Guides

**[memory-safety-guide.md](./references/memory-safety-guide.md)**
- Sanitizer basics (ASan, MSan, UBSan, TSan)
- Diagnosis workflow (reproduce → isolate → analyze → fix)
- Prevention strategies (code review, static analysis, testing)
- Multi-threaded memory issues (data races, deadlocks)
- Qt/QML-specific concurrency patterns
- CI/CD integration examples
- Tools reference

**[cpp-memory-patterns.md](./references/cpp-memory-patterns.md)**
- Common memory error patterns with code examples
- Leak, use-after-free, double-free, buffer overflow, uninitialized memory
- Qt/QML-specific patterns
- Smart pointer selection guide
- Testing strategies
- CppCoreGuidelines references

### Helper Scripts

**[scripts/asan-analyzer.sh](./scripts/asan-analyzer.sh)**
Parse ASan output and generate structured reports.
```bash
ctest --preset macos-asan-test-qt 2>&1 | tee asan.log
bash ./scripts/asan-analyzer.sh asan.log report.md
```

**[scripts/leak-reporter.sh](./scripts/leak-reporter.sh)**
Extract and summarize memory leaks from sanitizer output.
```bash
bash ./scripts/leak-reporter.sh asan.log leaks-report.md
```

**[scripts/run-all-sanitizers.sh](./scripts/run-all-sanitizers.sh)**
Run tests under all available sanitizers (ASan, MSan, UBSan) and generate a summary report.
```bash
bash ./scripts/run-all-sanitizers.sh ./sanitizer-reports
```

## Common Workflows

### Investigate a Memory Leak

```bash
# 1. Build with ASan
cmake --build --preset macos-asan-build-qt

# 2. Run tests
ctest --preset macos-asan-test-qt 2>&1 | tee asan.log

# 3. Analyze the report
bash .agents/skills/memory-safety-check/scripts/leak-reporter.sh asan.log

# 4. Locate the allocation site in the output
# 5. Fix: add delete, use unique_ptr, or refactor ownership
# 6. Re-run tests to verify fix
```

### Debug Use-After-Free

```bash
# 1. Build with ASan (includes use-after-free detection)
cmake --build --preset macos-asan-build-qt

# 2. Run the failing test with verbose output
ctest --preset macos-asan-test-qt -R TestName -V

# 3. Look for "heap-use-after-free" in output
# 4. Stack trace shows:
#    - Where object was freed (first)
#    - Where it was accessed after-free (bottom)
# 5. Fix: ensure object lifetime outlives all uses
# 6. Use unique_ptr or shared_ptr to manage lifetime
```

### Validate Code Before Commit

```bash
# Run all sanitizers
bash .agents/skills/memory-safety-check/scripts/run-all-sanitizers.sh ./reports

# Review reports
open ./reports/INDEX.md
```

### Add to CI/CD

Copy the GitHub Actions workflow from the [SKILL.md](./SKILL.md#cicd) section into `.github/workflows/memory-safety.yml`.

## Integration with Other Skills

- **qt-cpp-review**: Does comprehensive code review including memory safety patterns
- **code-review**: Structural review before memory testing
- **diagnosing-bugs**: Deep analysis when sanitizer output is unclear

## Prerequisites

- CMake with sanitizer support enabled (see `cmake/EnableSanitizers.cmake`)
- Compiler that supports sanitizers (Clang/GCC)
- Build presets: `macos-asan-build-qt`, `macos-asan-test-qt` (or platform equivalent)

## Example: End-to-End Fix

**Issue**: Application crashes after several operations; memory leak suspected.

```bash
# Step 1: Run ASan tests
cmake --build --preset macos-asan-build-qt
ctest --preset macos-asan-test-qt 2>&1 | tee asan.log

# Step 2: Analyze output
bash .agents/skills/memory-safety-check/scripts/asan-analyzer.sh asan.log

# Step 3: See report shows leak in MyClass constructor
# File: src/myclass.cpp:45
# Allocation: new char[4096]

# Step 4: Fix the code
# Find the destructor: replace 'delete[] buffer' logic
# Use unique_ptr instead:
# Before: char* buffer; // in destructor: delete[] buffer;
# After:  std::unique_ptr<char[]> buffer;

# Step 5: Verify fix
cmake --build --preset macos-asan-build-qt
ctest --preset macos-asan-test-qt

# Step 6: Confirm no leaks
bash .agents/skills/memory-safety-check/scripts/leak-reporter.sh asan.log
# Output: "No leaks found" ✅
```

## Further Reading

- [AddressSanitizer Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [CppCoreGuidelines](https://github.com/isocpp/CppCoreGuidelines)
- [Qt Object Model](https://doc.qt.io/qt-6/object.html)
- [Qt Memory Management](https://doc.qt.io/qt-6/objecttrees.html)

## Questions?

Ask the agent:
- "Run memory safety checks"
- "Analyze this memory leak"
- "How do I fix use-after-free?"
- "What's the difference between ASan and MSan?"
