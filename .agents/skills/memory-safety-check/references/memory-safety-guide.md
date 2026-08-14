# Memory Safety Comprehensive Guide

Deep dive into memory safety concepts, diagnostics, and prevention strategies for C++ projects.

## Sanitizer Basics

### Address Sanitizer (ASan)

Detects buffer overflows, use-after-free, double-free, and memory leaks at runtime.

**Enable in CMake:**
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
```

**Runtime options:**
```bash
# Detect leaks at exit
ASAN_OPTIONS=detect_leaks=1 ./app

# Verbosity
ASAN_OPTIONS=verbosity=2:halt_on_error=0 ./app

# Suppressions file
ASAN_OPTIONS=suppressions=suppressions.txt ./app
```

**Output interpretation:**
```
ERROR: AddressSanitizer: heap-buffer-overflow on unknown address 0x62e00008 (T)
READ of size 4 at 0x62e00008 thread T0
    #0 0x... in function_name() at file.cpp:42
    #1 0x... in caller() at caller.cpp:99
```

- `READ` = read overflow; `WRITE` = write overflow
- Stack trace shows where the error occurred
- Line numbers pinpoint the exact location

### Memory Sanitizer (MSan)

Detects use of uninitialized memory.

**Enable in CMake:**
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory -g")
```

**Caveat**: Requires instrumented stdlib. Use with Clang + libc++:
```cmake
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory -stdlib=libc++ -g")
```

### Undefined Behavior Sanitizer (UBSan)

Detects undefined behavior (signed overflow, null dereferences, type mismatches, etc.).

**Enable in CMake:**
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -g")
```

**Common detections:**
```
runtime error: signed integer overflow: 2147483647 + 1 cannot be represented
runtime error: member access within null pointer of type 'struct MyClass'
runtime error: index 10 out of bounds for type 'int [10]'
```

---

## Diagnosis Workflow

### Step 1: Reproduce the Issue Consistently

Flaky memory bugs are hardest to fix. Before diagnosing:

1. Run the test/app multiple times
2. Try with different build flags (debug vs. release)
3. Try with different sanitizers (ASan vs. MSan vs. UBSan)
4. Try with different data inputs

**Script to run multiple times:**
```bash
#!/bin/bash
for i in {1..10}; do
    echo "Run $i..."
    ./build/macos-asan-qt/src/main/LogViewer.app/Contents/MacOS/LogViewer
done
```

### Step 2: Isolate the Failing Component

Narrow down which subsystem is failing:

- **Full test suite**: `ctest --preset macos-asan-test-qt`
- **Single test**: `ctest --preset macos-asan-test-qt -R TestName`
- **Manual reproduction**: `./app --specific-operation`

If narrowing to a single test, create a minimal reproduction case (see Step 3).

### Step 3: Create a Minimal Reproduction Test

Isolate the issue to the fewest lines of code:

```cpp
// tests/minimal_repro.cpp
#include <gtest/gtest.h>
#include "component.h"

TEST(MemorySafety, MinimalRepro) {
    // Minimal code that triggers the issue
    auto component = std::make_unique<Component>();
    component->doSomething();  // Issue happens here
    // Sanitizer should report error
}
```

Build and run with `-V` for verbose output:
```bash
ctest --preset macos-asan-test-qt -R MinimalRepro -V
```

### Step 4: Interpret the Sanitizer Report

Key sections:

```
ERROR: AddressSanitizer: heap-use-after-free
    Address 0x... is located inside of 64-byte region
    freed by thread T0 here:
        #0 0x... operator delete[](void*) ...
        #1 0x... Destructor at file.cpp:50
    previously allocated here:
        #0 0x... operator new[](unsigned long) ...
        #1 0x... Constructor at file.cpp:10
    accessed here:
        #0 0x... MyClass::process() at file.cpp:42
        #1 0x... main() at main.cpp:99
```

**Read as:**
- Object allocated at file.cpp:10
- Object freed at file.cpp:50 (in destructor)
- Object accessed after-free at file.cpp:42
- Root cause: file.cpp:42 is called after destruction

### Step 5: Identify the Root Cause

Common patterns:

| Symptom | Likely Cause |
|---------|--------------|
| Use-after-free in destructor | Double-delete or destroyed while still referenced |
| Leak report with no clear allocation | Global object or static initialization order |
| Buffer overflow in library call | Buffer size mismatch with API |
| Uninitialized field used in condition | Field not set on all code paths |

---

## Prevention Strategies

### 1. Code Review Checklist

Before submitting code, verify:

- [ ] All `new` has matching `delete` (or use `unique_ptr`)
- [ ] No pointer assignments without lifetime verification
- [ ] Container deletions don't invalidate external pointers
- [ ] Constructors initialize all fields
- [ ] Destructors don't double-free
- [ ] Callbacks checked for dangling pointers
- [ ] Qt signals/slots have matching lifetimes

### 2. Static Analysis Tools

Run linters before tests:

**Clang-Tidy:**
```bash
cmake --build --preset macos-checks-build-qt
cmake --build --preset macos-clang-tidy
```

**Cppcheck:**
```bash
cmake --build --preset macos-cppcheck
```

These catch some patterns without running code (faster, no data-dependent issues).

### 3. Compile-Time Warnings

Enable strict warnings in CMake:

```cmake
if (MSVC)
    target_compile_options(MyTarget PRIVATE /W4)
else()
    target_compile_options(MyTarget PRIVATE -Wall -Wextra -Wpedantic)
    # Enable specific warnings
    target_compile_options(MyTarget PRIVATE
        -Wuninitialized
        -Wdouble-promotion
        -Wshadow
    )
endif()
```

### 4. Testing Strategy

Write tests that exercise error paths:

```cpp
TEST(MemorySafety, ErrorPathCleanup) {
    // Trigger error condition; verify cleanup
    auto obj = std::make_unique<Component>();
    EXPECT_THROW(obj->failingOperation(), std::runtime_error);
    // ASan verifies no leak
}

TEST(MemorySafety, ConcurrentAccess) {
    // Thread safety
    auto obj = std::make_shared<Component>();
    std::thread t1([obj]() { obj->read(); });
    std::thread t2([obj]() { obj->modify(); });
    t1.join();
    t2.join();
    // ASan + ThreadSanitizer catch races
}
```

### 5. Design Patterns

**RAII (Resource Acquisition Is Initialization):**
- Allocate in constructor
- Deallocate in destructor
- Smart pointers enforce this

**Ownership models:**
- Clearly document who owns what
- Use `unique_ptr` for exclusive ownership
- Use `shared_ptr` when multiple owners needed
- Use `weak_ptr` for non-owning observers

---

## CI/CD Integration

### GitHub Actions Workflow

```yaml
name: Memory Safety

on: [push, pull_request]

jobs:
  asan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build with ASan
        run: |
          cmake --preset linux-asan-build
          cmake --build --preset linux-asan-build
      - name: Run tests with ASan
        run: |
          ctest --preset linux-asan-test --output-on-failure
        env:
          ASAN_OPTIONS: detect_leaks=1:halt_on_error=1

  msan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build with MSan
        run: |
          cmake --preset linux-msan-build
          cmake --build --preset linux-msan-build
      - name: Run tests with MSan
        run: |
          ctest --preset linux-msan-test --output-on-failure

  ubsan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build with UBSan
        run: |
          cmake --preset linux-ubsan-build
          cmake --build --preset linux-ubsan-build
      - name: Run tests with UBSan
        run: |
          ctest --preset linux-ubsan-test --output-on-failure
```

### Local Pre-Commit Hook

```bash
#!/bin/bash
# .git/hooks/pre-commit

echo "Running memory safety checks..."
cmake --build --preset macos-asan-build-qt || exit 1
ctest --preset macos-asan-test-qt || exit 1
echo "✅ Memory safety checks passed"
```

---

## Multi-Threaded Memory Issues

### Data Races

**Symptom**: `ThreadSanitizer: data race`

**Detection**: Enable ThreadSanitizer (TSan) in CMake:
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")
```

**Common patterns:**

```cpp
// ❌ Data race
class Counter {
    int value = 0;
public:
    void increment() { value++; }  // Race: read-modify-write
    int get() { return value; }
};

// ✅ Safe
class Counter {
    std::atomic<int> value{0};
public:
    void increment() { value++; }  // Atomic operation
    int get() { return value; }
};
```

### Lock-Induced Deadlock

**Symptom**: Timeout waiting for mutex; possible deadlock

**Detection**: Enable Helgrind (part of Valgrind):
```bash
valgrind --tool=helgrind ./app
```

**Common patterns:**

```cpp
// ❌ Potential deadlock: inconsistent lock order
class Manager {
    std::mutex mu1, mu2;
    
    void operation1() {
        std::lock_guard<std::mutex> l1(mu1);
        std::lock_guard<std::mutex> l2(mu2);  // Lock order: mu1 then mu2
    }
    
    void operation2() {
        std::lock_guard<std::mutex> l2(mu2);
        std::lock_guard<std::mutex> l1(mu1);  // Lock order: mu2 then mu1 -> DEADLOCK!
    }
};

// ✅ Safe: always lock in same order
class Manager {
    std::mutex mu1, mu2;
    
    void operation1() {
        std::lock_guard<std::mutex> l1(mu1);
        std::lock_guard<std::mutex> l2(mu2);
    }
    
    void operation2() {
        std::lock_guard<std::mutex> l1(mu1);
        std::lock_guard<std::mutex> l2(mu2);
    }
};

// ✅ Or use std::lock
std::lock(mu1, mu2);
```

---

## Qt/QML-Specific Concurrency

### QThread and QObject Lifetime

```cpp
// ❌ Danger: QObject destroyed while thread running
class Worker : public QObject {
    Q_OBJECT
public:
    void startWork() {
        auto thread = new QThread();
        moveToThread(thread);
        connect(thread, &QThread::started, this, &Worker::doWork);
        thread->start();
        // thread is orphaned; if deleted before done, crash!
    }
};

// ✅ Safe: manage thread lifetime
class WorkerManager {
    std::unique_ptr<QThread> m_thread;
    Worker m_worker;
    
public:
    WorkerManager() {
        m_thread = std::make_unique<QThread>();
        m_worker.moveToThread(m_thread.get());
        connect(m_thread.get(), &QThread::started, &m_worker, &Worker::doWork);
        m_thread->start();
        // Thread lifetime tied to manager; proper cleanup
    }
    
    ~WorkerManager() {
        m_worker.quit();  // Signal thread to stop
        m_thread->wait();  // Wait for thread to finish
    }
};
```

---

## Tools & Commands Reference

| Command | Purpose |
|---------|---------|
| `ctest --preset macos-asan-test-qt` | Run tests with ASan |
| `ctest --preset macos-msan-test-qt` | Run tests with MSan |
| `ctest --preset macos-ubsan-test-qt` | Run tests with UBSan |
| `ASAN_OPTIONS=detect_leaks=1 ./app` | Force leak detection |
| `valgrind --leak-check=full ./app` | Deep leak analysis (slower) |
| `valgrind --tool=helgrind ./app` | Thread race detector |
| `cmake --build --preset macos-clang-tidy` | Static analysis |

---

## Further Reading

- **CppCoreGuidelines**: https://github.com/isocpp/CppCoreGuidelines
- **AddressSanitizer**: https://github.com/google/sanitizers/wiki/AddressSanitizer
- **ThreadSanitizer**: https://github.com/google/sanitizers/wiki/ThreadSanitizer
- **Qt Object Model**: https://doc.qt.io/qt-6/object.html
- **Qt Threading**: https://doc.qt.io/qt-6/thread-support.html
