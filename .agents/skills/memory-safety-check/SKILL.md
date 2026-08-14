---
name: memory-safety-check
description: 'Comprehensive workflow to verify memory safety and resource leaks in C++ code using Address Sanitizer (ASan), Memory Sanitizer (MSan), and Undefined Behavior Sanitizer (UBSan). Use for: investigating memory leaks, buffer overflows, use-after-free bugs, double-frees, data races, invalid memory access, or validating fixes before release. Trigger phrases: "check memory safety", "run ASan", "verify memory leaks", "debug memory issues", "validate ASan", "analyze sanitizer output".'
argument-hint: '[optional: specific test name or code path to focus on]'
user-invocable: true
---

# Memory & Resource Safety Check

This skill guides you through building, running, and analyzing C++ code under Sanitizers (ASan, MSan, UBSan) to catch memory safety issues that could crash or corrupt the application in production.

## When to Use

- **Investigating memory leaks**: "The app seems to be leaking memory"
- **Buffer overflows or invalid access**: "Crashes in production, works in debug"
- **Use-after-free or double-free**: "Memory corruption errors"
- **Data races**: "Undefined behavior under concurrency"
- **Undefined behavior**: "Unpredictable crashes or behavior"
- **Before release**: "Validate code for safety"
- **Reviewing changes**: "Ensure new code doesn't introduce leaks/overflows"

## Prerequisites

- CMake configured for sanitizer support (see `EnableSanitizers.cmake`)
- Build presets available: `macos-asan-build-qt`, `macos-msan-build-qt` (or platform equivalent)
- Test suite in place and passing

## Procedure

### 1. **Prepare the Build Environment** {#step1}

Decide which sanitizer(s) to use:

| Sanitizer | Detects | Use Case | Overhead |
|-----------|---------|----------|----------|
| **ASan** | Buffer overflows, use-after-free, double-free, leaks | Most common, recommended first | 2–3× |
| **MSan** | Uninitialized memory reads | When ASan doesn't find issues | 3–4× |
| **UBSan** | Undefined behavior (signed overflow, null ptr, etc.) | Strict correctness checking | 1–2× |
| **ASan + UBSan** | Combined (ASan is primary) | Most comprehensive | 3–4× |

### 2. **Build with Sanitizers**

Run the ASan build configuration:

```bash
# Terminal: run the build task
Build: ASan
```

Monitor for build errors. If the build fails:
- Check `cmake` output for unsupported compiler flags
- Ensure your compiler (Clang/GCC) supports sanitizers
- Review `EnableSanitizers.cmake` configuration

**Outcome**: Built executable under `build/macos-asan-qt/`

### 3. **Run Tests Under Sanitizers**

Execute the full test suite with sanitizer instrumentation:

```bash
# Terminal: run the test task
Test: ASan
```

Sanitizers will execute all tests and report issues in real-time.

**Watch for**:
- Errors starting with `ERROR: AddressSanitizer:` (buffer overflow, use-after-free, etc.)
- Memory leak summaries at test exit
- Undefined behavior warnings from UBSan
- Data race reports (if TSan is enabled)

### 4. **Capture and Analyze Output**

If the test run completes, collect the output:

```bash
# Copy test output to a file for analysis
ctest --preset macos-asan-test-qt 2>&1 | tee asan-output.txt
```

Or, run the application directly to reproduce a specific issue:

```bash
# Run the debug build with ASan
./build/macos-asan-qt/src/main/LogViewer.app/Contents/MacOS/LogViewer [args]
```

**Analyze the report**:
- Note the **error type** (buffer-overflow, heap-use-after-free, memory-leak, etc.)
- Identify the **stack trace** showing where the error occurred
- Look for **source file and line number** in the trace

### 5. **Identify Patterns in Errors**

Group similar issues:

- **Memory leaks**: All point to the same allocation site? Same leak, multiple code paths?
- **Use-after-free**: Freed in one function, accessed in another?
- **Overflows**: Buffer size mismatch or incorrect bounds checking?
- **Uninitialized memory**: Reads before writes, or paths that skip initialization?

Create notes on recurring patterns:

```
Pattern: [Issue Type]
  Location: [file:line]
  Trigger: [what causes it]
  Impact: [crash, corruption, leak]
  Root cause hypothesis: [theory]
```

### 6. **Reproduce Issue in Isolation** *(if needed)*

For flaky or hard-to-reproduce issues, create a minimal test case:

```cpp
// tests/memory_safety_check_issue_X.cpp
#include <gtest/gtest.h>
#include "component_under_test.h"

TEST(MemorySafetyCheck, ReproduceIssueX) {
    // Minimal steps to trigger the issue
    auto obj = std::make_unique<ComponentUnderTest>();
    obj->triggeringOperation();  // Sanitizer should report error here
}
```

Run just this test:

```bash
ctest --preset macos-asan-test-qt -R ReproduceIssueX -V
```

### 7. **Fix the Issue**

Common fixes for memory safety issues:

| Error Type | Common Fix |
|------------|-----------|
| **Buffer overflow** | Increase buffer size or add bounds check |
| **Use-after-free** | Ensure object isn't freed while still in use; use smart pointers |
| **Memory leak** | Find missing `delete`/`free` or wrap in `std::unique_ptr`/`std::shared_ptr` |
| **Double-free** | Use smart pointers; ensure cleanup happens exactly once |
| **Uninitialized memory** | Initialize variables before use; check all code paths |
| **Data race** | Add mutex protection or make thread-local; ensure happens-before edges |

Example fix pattern:

```cpp
// Before (leak)
void process() {
    Data* data = new Data();
    if (error) return;  // Leak!
    delete data;
}

// After (safe)
void process() {
    auto data = std::make_unique<Data>();
    if (error) return;  // No leak, unique_ptr cleans up
    // Use data...
}
```

### 8. **Verify the Fix**

Rebuild with ASan and re-run the test:

```bash
# Rebuild
Build: ASan

# Run the specific test or full suite
Test: ASan
```

Confirm:
- The specific error is gone
- No new errors appeared
- Memory leak summary shows reduced leaks or zero

### 9. **Extended Testing: Run with Multiple Sanitizers**

For thorough validation, test with complementary sanitizers:

```bash
# Memory Sanitizer (uninitialized memory)
cmake --preset macos-msan-build-qt
cmake --build --preset macos-msan-build-qt
ctest --preset macos-msan-test-qt

# Undefined Behavior Sanitizer
cmake --preset macos-ubsan-build-qt  # if available
cmake --build --preset macos-ubsan-build-qt
ctest --preset macos-ubsan-test-qt
```

### 10. **Validate Against Valgrind** *(optional, deeper check)*

For critical code, optionally validate with Valgrind (more thorough but slower):

```bash
valgrind --leak-check=full --show-leak-kinds=all \
  ./build/macos-asan-qt/src/main/LogViewer.app/Contents/MacOS/LogViewer
```

Compare findings with ASan results. If Valgrind reports issues ASan missed, investigate further.

### 11. **Document Findings and Lessons**

Record what was fixed:

```markdown
## Issue: [Name]
- **Error Type**: [ASan error type]
- **Location**: [file:line]
- **Root Cause**: [brief explanation]
- **Fix**: [what was changed]
- **Test Coverage**: [how to catch this in future]
- **Preventive Measures**: [code review checklist item]
```

Store in project wiki or issue tracker for team reference.

### 12. **Update Code Review Checklist** *(optional)*

If you discovered a new class of bug, add it to:
- PR review checklist
- Pre-commit hook
- Code style guide

This ensures similar issues don't re-appear.

## Common Sanitizer Output Examples

### Memory Leak
```
SUMMARY: AddressSanitizer: 48 byte(s) leaked in 1 allocation(s).
```
**Fix**: Find the `new`/`malloc` without matching `delete`/`free`.

### Use-After-Free
```
ERROR: AddressSanitizer: heap-use-after-free on unknown address ...
    #0 0x... in MyClass::process() at file.cpp:123
    #1 0x... in main() at main.cpp:456
```
**Fix**: Object is freed before this line. Use smart pointers or refactor ownership.

### Buffer Overflow
```
ERROR: AddressSanitizer: buffer-overflow on unknown address ...
    Write of size 256 at offset 100 (buffer is 200 bytes)
```
**Fix**: Buffer is too small; increase size or reduce write.

### Uninitialized Memory
```
ERROR: MemorySanitizer: use-of-uninitialized-value
    #0 0x... in MyClass::getData() at file.cpp:99
```
**Fix**: Initialize all fields in constructor or before use.

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Build fails with "unsupported -fsanitize" | Your compiler doesn't support sanitizers; use Clang/GCC |
| Tests pass with ASan but crash in Release | ASan only checks in instrumented build; still has bugs |
| Too many false positives in third-party code | Suppress with `LSAN_OPTIONS=suppressions=...` or link with unsanitized libs |
| ASan is too slow | Use UBSan alone for faster runs; reduce test scope with `-R` filter |
| Leak reports are noisy | Use `LeakSanitizer` options to filter by allocation site |

## References

- [Address Sanitizer (ASAN) docs](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [CMake EnableSanitizers.cmake](../../cmake/EnableSanitizers.cmake)
- [Memory safety best practices](./references/memory-safety-guide.md)
- [Common C++ memory errors and fixes](./references/cpp-memory-patterns.md)

## Extended Topics

### Concurrency & Thread Safety {#concurrency}

Multi-threaded code introduces additional memory safety challenges.

#### Thread Sanitizer (TSan)

Detects data races in concurrent code.

**Enable in CMake:**
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")
```

**Common race pattern:**
```cpp
// ❌ Data race
class Counter {
    int value = 0;  // Shared, unprotected
public:
    void increment() { value++; }  // Read-modify-write without synchronization
};

// ✅ Safe
class Counter {
    std::atomic<int> value{0};
public:
    void increment() { value++; }  // Atomic operation, no race
};
```

**Workflow to detect races:**
```bash
# Build with ThreadSanitizer
cmake --preset macos-tsan-build-qt  # if available
cmake --build --preset macos-tsan-build-qt

# Run tests
ctest --preset macos-tsan-test-qt
```

**Interpret TSan output:**
```
WARNING: ThreadSanitizer: data race
    Write of size 4 at 0x... by thread T2:
        #0 Counter::increment() at counter.cpp:10
    Previous read at 0x... by thread T1:
        #0 Counter::get() at counter.cpp:12
```

**Prevention:**
- Use `std::atomic<T>` for shared primitives
- Use `std::mutex` + `std::lock_guard` for complex shared state
- Use thread-local storage (`thread_local`) when possible
- Verify consistent lock ordering to prevent deadlock

#### Deadlock Detection

**Tool**: Helgrind (part of Valgrind)

```bash
valgrind --tool=helgrind ./build/macos-debug-qt/src/main/LogViewer
```

**Common deadlock pattern:**
```cpp
// ❌ Potential deadlock: inconsistent lock order
class Manager {
    std::mutex mu1, mu2;
    
    void taskA() {
        std::lock_guard<std::mutex> l1(mu1);
        std::lock_guard<std::mutex> l2(mu2);  // Order: mu1 → mu2
    }
    
    void taskB() {
        std::lock_guard<std::mutex> l2(mu2);
        std::lock_guard<std::mutex> l1(mu1);  // Order: mu2 → mu1 → DEADLOCK!
    }
};

// ✅ Safe: Always lock in same order
class Manager {
    std::mutex mu1, mu2;
    
    void taskA() {
        std::lock_guard<std::mutex> l1(mu1);
        std::lock_guard<std::mutex> l2(mu2);
    }
    
    void taskB() {
        std::lock_guard<std::mutex> l1(mu1);  // Same order
        std::lock_guard<std::mutex> l2(mu2);
    }
};
```

### Qt/QML Memory Patterns {#qt-qml}

Qt has its own memory management model that interacts with C++ safety checks.

#### Qt Object Ownership

Qt objects use parent-child relationships for automatic cleanup:

```cpp
// ✅ Safe: Qt manages lifetime
class MyDialog : public QDialog {
public:
    MyDialog() {
        auto button = new QPushButton("OK", this);  // this is parent
        // When MyDialog is destroyed, button is automatically deleted
    }
};

// ❌ Danger: Orphaned QObject
class MyDialog : public QDialog {
public:
    MyDialog() {
        auto button = new QPushButton("OK");  // No parent!
        // Memory leak: button is never deleted
    }
};
```

**Best practice:** Use Qt parent-child model or `unique_ptr`:
```cpp
class MyDialog : public QDialog {
    std::unique_ptr<QPushButton> m_button;
public:
    MyDialog() {
        m_button = std::make_unique<QPushButton>("OK");
        // Button lifetime tied to dialog
    }
};
```

#### QObject Signals & Slots Across Lifetimes

When connecting signals to slots, ensure objects don't outlive each other:

```cpp
// ❌ Danger: receiver deleted while emitter still connected
class DataModel : public QObject {
    Q_SIGNALS:
        void dataChanged();
};

class View : public QObject {
public:
    View(DataModel* model) {
        connect(model, &DataModel::dataChanged, this, &View::refresh);
        // If model is deleted while View exists, signal causes crash
    }
    Q_SLOT void refresh() { /* */ }
};

// ✅ Safe: use shared_ptr or ensure listener deleted first
class View : public QObject {
public:
    View(std::shared_ptr<DataModel> model)
        : m_model(model) {
        connect(m_model.get(), &DataModel::dataChanged,
                this, &View::refresh,
                Qt::AutoConnection);  // Qt auto-disconnects if receiver deleted
    }
private:
    std::shared_ptr<DataModel> m_model;
    Q_SLOT void refresh() { /* */ }
};
```

#### QML-C++ Memory Issues

QML engines manage C++ object lifetimes; be careful with ownership:

```cpp
// ❌ Danger: C++ deletes object while QML still holds reference
class DataProvider : public QObject {
public:
    Q_INVOKABLE QString getData() { return "data"; }
};

class App {
public:
    void setupQML() {
        auto provider = std::make_unique<DataProvider>();
        engine->rootContext()->setContextProperty("provider", provider.get());
        // provider is deleted here, but QML still references it!
    }
};

// ✅ Safe: keep object alive
class App : public QObject {
    std::unique_ptr<DataProvider> m_provider;
public:
    void setupQML() {
        m_provider = std::make_unique<DataProvider>();
        engine->rootContext()->setContextProperty("provider", m_provider.get());
        // m_provider remains alive as long as App exists
    }
};
```

#### QVariant with Pointers

Never store raw pointers in QVariant; use shared_ptr or models:

```cpp
// ❌ Danger: QVariant may outlive pointer
auto model = new MyModel();
QVariant var = QVariant::fromValue((void*)model);
delete model;
auto ptr = var.value<void*>();  // ptr is dangling!

// ✅ Safe: store in model
class DataStore : public QAbstractListModel {
    QVector<std::shared_ptr<Item>> m_items;
};
```

### CI/CD Integration {#cicd}

Integrate sanitizer testing into your continuous integration pipeline.

#### GitHub Actions Workflow

```yaml
name: Memory Safety Checks

on: [push, pull_request]

jobs:
  asan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Build with ASan
        run: |
          cmake --preset linux-asan-build-qt
          cmake --build --preset linux-asan-build-qt
      
      - name: Run tests with ASan
        run: |
          ctest --preset linux-asan-test-qt --output-on-failure
        env:
          ASAN_OPTIONS: detect_leaks=1:halt_on_error=1
      
      - name: Analyze results
        if: failure()
        run: |
          bash .agents/skills/memory-safety-check/scripts/asan-analyzer.sh

  ubsan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build with UBSan
        run: |
          cmake --preset linux-ubsan-build-qt
          cmake --build --preset linux-ubsan-build-qt
      - name: Run tests with UBSan
        run: ctest --preset linux-ubsan-test-qt --output-on-failure
```

#### Local Pre-Commit Hook

```bash
#!/bin/bash
# .git/hooks/pre-commit

echo "🔬 Running memory safety checks..."

# Build with ASan
cmake --build --preset macos-asan-build-qt || exit 1

# Run tests
ctest --preset macos-asan-test-qt --output-on-failure || exit 1

echo "✅ Memory safety checks passed"
```

Install with:
```bash
chmod +x .git/hooks/pre-commit
```

#### Helper Scripts

The skill includes automation scripts in `./scripts/`:

| Script | Purpose |
|--------|---------|
| `asan-analyzer.sh` | Parse ASan output and generate HTML/Markdown report |
| `leak-reporter.sh` | Extract and summarize memory leaks |
| `run-all-sanitizers.sh` | Run tests under all available sanitizers; generate summary |

**Usage:**
```bash
# Run all sanitizers and generate reports
bash ./scripts/run-all-sanitizers.sh ./reports

# Analyze a specific test run
ctest --preset macos-asan-test-qt 2>&1 | tee asan.log
bash ./scripts/asan-analyzer.sh asan.log report.md
```

## Related Skills

- **code-review**: Structural review of code changes before sanitizer testing
- **diagnosing-bugs**: Deeper analysis when sanitizer output is complex
- **qt-cpp-review**: Integrated review combining linting + sanitizer verification
