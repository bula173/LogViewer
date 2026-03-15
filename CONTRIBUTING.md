# Contributing to LogViewer

Thank you for your interest in contributing to LogViewer! This guide will help you get started with development and understand our contribution process.

## Code of Conduct

We are committed to providing a welcoming and inclusive environment. Please treat all contributors with respect.

---

## Getting Started

### Prerequisites

- **C++20 Compiler**: GCC 11+, Clang 15+, MSVC 2019+, or Apple Clang 15+
- **CMake**: 3.25 or later
- **Qt**: 6.5 or later
- **Git**: Latest version

### Development Setup

1. **Fork and Clone**
```bash
git clone https://github.com/YOUR_USERNAME/LogViewer.git
cd LogViewer
```

2. **Create Development Branch**
```bash
git checkout -b feature/my-feature
```

3. **Build Project**
```bash
# macOS/Linux
cmake --build --preset macos-debug-build-qt -j$(nproc)

# Or use your platform preset
cmake --build --preset <platform>-debug-build-qt
```

4. **Run Tests**
```bash
./build/macos-debug-qt/tests/bin/LogViewer_tests
```

5. **Run Application**
```bash
./build/macos-debug-qt/src/main/LogViewer
```

---

## Code Style Guidelines

### C++ Standards

- **C++20** is required and expected
- Use modern C++ features appropriately:
  - Structured bindings: `for (const auto& [key, value] : items)`
  - std::optional for optional values
  - std::ranges:: for algorithms
  - std::concepts for template constraints
  - Perfect forwarding with `std::forward<>`

### Naming Conventions

**Classes and Structs**
```cpp
class EventsContainer { };
struct PluginMetadata { };
```

**Functions and Methods**
```cpp
void LoadPlugin();
int GetEventCount() const;
bool IsValid() const;
```

**Member Variables**
```cpp
class MyClass {
private:
    int m_count;
    std::string m_name;
    std::vector<Item> m_items;
};
```

**Constants**
```cpp
constexpr int DEFAULT_BUFFER_SIZE = 1024;
static const std::string CONFIG_FILENAME = "config.json";
```

### Auto-Format Code

Use clang-format to maintain consistent style:

```bash
# Format a single file
clang-format -i src/application/MyFile.cpp

# Format all source files
find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Check formatting without modifying
clang-format --dry-run -Werror src/application/MyFile.cpp
```

Configuration is in `.clang-format` (WebKit style).

---

## Doxygen Documentation Requirements

### Header Files

All public classes, functions, and structs must have Doxygen comments:

```cpp
/**
 * @class MyClass
 * @brief Brief description of the class.
 *
 * Detailed description explaining what the class does,
 * how it works, and usage information.
 *
 * @par Thread Safety
 * This class is thread-safe for concurrent reads but not writes.
 *
 * @par Example
 * @code
 * MyClass obj;
 * obj.Initialize();
 * @endcode
 *
 * @see RelatedClass for related functionality
 */
class MyClass {
public:
    /**
     * @brief Initializes the object.
     *
     * @param name The object name
     * @return Result<void, Error> Success or error
     *
     * @throw std::invalid_argument if name is empty
     *
     * @par Complexity
     * O(1)
     */
    Result<void, Error> Initialize(std::string_view name);
};
```

### Markdown Files

Create `.md` files for major features:
- Place in `docs/` directory
- Use H1 (#) for main title, H2 (##) for sections
- Include code examples where helpful
- Link to related documentation

---

## Testing Requirements

### Writing Tests

All new features must include tests:

```cpp
#include <gtest/gtest.h>
#include "MyFeature.hpp"

TEST(MyFeatureTest, BasicFunctionality) {
    MyFeature feature;
    EXPECT_TRUE(feature.Initialize());
}

TEST(MyFeatureTest, ErrorHandling) {
    MyFeature feature;
    auto result = feature.OperateOnInvalidData();
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ErrorCode::INVALID_DATA);
}
```

### Running Tests

```bash
# Run all tests
./build/macos-debug-qt/tests/bin/LogViewer_tests

# Run specific test
./build/macos-debug-qt/tests/bin/LogViewer_tests --gtest_filter=MyFeatureTest.BasicFunctionality

# Run tests and see detailed output
./build/macos-debug-qt/tests/bin/LogViewer_tests --gtest_verbose
```

### Test Coverage

- New features should have >80% code coverage
- Critical paths (error handling, threading) require thorough testing
- Use mocks for external dependencies

---

## Common Contribution Types

### Bug Fixes

1. **Create Issue** (if not already reported)
2. **Create Branch**: `git checkout -b fix/issue-description`
3. **Fix the Bug** with test case demonstrating the fix
4. **Run Tests**: Ensure all tests pass
5. **Create Pull Request** referencing the issue

Example commit message:
```
Fix: Prevent null pointer in EventsContainer::GetEvent()

- Added bounds checking before accessing event array
- Added test case for out-of-bounds access
- Fixes #123
```

### New Features

1. **Discuss Design** - Create an issue to discuss approach
2. **Create Branch**: `git checkout -b feature/feature-name`
3. **Implement Feature** with:
   - Code following style guidelines
   - Comprehensive Doxygen documentation
   - Unit tests with good coverage
   - Example usage in API_EXAMPLES.md if applicable
4. **Add Tests** with good coverage
5. **Create Pull Request** with detailed description

Example commit message:
```
Feature: Add regex filter matching strategy

- Created RegexFilterStrategy implementing IFilterStrategy
- Supports case-sensitive and case-insensitive matching
- Added 5 test cases covering common patterns
- Updated API_EXAMPLES.md with usage
```

### Documentation Improvements

1. **Create Branch**: `git checkout -b docs/improvement-description`
2. **Update Documentation**:
   - Fix typos and clarify language
   - Add missing examples
   - Update architecture diagrams if needed
3. **Create Pull Request** with explanation of changes

---

## Pull Request Process

### Before Submitting PR

```bash
# 1. Update your branch
git fetch origin
git rebase origin/main

# 2. Build and test
cmake --build --preset macos-debug-build-qt -j$(nproc)
./build/macos-debug-qt/tests/bin/LogViewer_tests

# 3. Check code style
find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format --dry-run -Werror

# 4. Run static analysis
cppcheck --enable=all src/
```

### PR Description Template

```markdown
## Description
Brief explanation of what this PR does.

## Type
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation
- [ ] Performance improvement

## Testing
Describe testing performed:
- Unit tests added: X new tests
- Manual testing: ...
- Regression testing: ...

## Checklist
- [ ] Code follows style guidelines
- [ ] Tests added/updated
- [ ] Documentation updated
- [ ] No new compiler warnings
- [ ] All tests pass
```

### Review Process

1. **Automated Checks**
   - GitHub Actions CI must pass (build, tests, linting)
   - Codeql analysis must pass

2. **Code Review**
   - At least 1 maintainer approval required
   - Address all feedback or mark as resolved with explanation

3. **Merge**
   - Squash commits for clarity (maintainers can assist)
   - Delete branch after merge

---

## Code Quality Standards

### Quality Checks

```bash
# Format check
clang-format --dry-run -Werror src/my_file.cpp

# Static analysis
cppcheck --enable=all src/my_file.cpp

# Compiler warnings
cmake --build --preset <preset> 2>&1 | grep -i warning

# Unit tests
./build/<preset>/tests/bin/LogViewer_tests
```

### Requirements for Merge

- ✅ All tests pass (45+ tests)
- ✅ No new compiler warnings
- ✅ Code formatted with clang-format
- ✅ Doxygen comments for public API
- ✅ No cppcheck errors
- ✅ GitHub Actions CI passes

---

## Performance Considerations

### Optimization Guidelines

1. **Profile Before Optimizing**
   - Use debugger to identify bottlenecks
   - Don't optimize without evidence

2. **Prefer Readability**
   - Clear code > Clever code
   - Use modern C++ for clarity

3. **Thread Safety**
   - Use shared_mutex for readers-writers
   - Document thread-safety guarantees
   - Avoid shared state when possible

4. **Memory Usage**
   - Use move semantics and perfect forwarding
   - Prefer stack allocation when possible
   - Use smart pointers (unique_ptr, shared_ptr)

---

## Common Pitfalls

### Don't

- ❌ Use `nullptr` comparisons: Use `if (!ptr)` instead
- ❌ Use C-style casts: Use `static_cast<>`, `dynamic_cast<>`, etc.
- ❌ Catch exceptions by value: Use `const std::exception&`
- ❌ Commit debug code or temporary changes
- ❌ Make unrelated changes in a single PR

### Do

- ✅ Use structured bindings: `for (auto& [k, v] : map)`
- ✅ Use std::string_view for read-only string parameters
- ✅ Use std::optional{} for optional values
- ✅ Use Result<T, E> for fallible operations
- ✅ Keep commits focused and logical

---

## Getting Help

### Questions?

- **Issues**: Use GitHub Issues for bugs and feature requests
- **Discussions**: Use GitHub Discussions for questions
- **Slack/Discord**: Join the community chat (if available)

### Resources

- [DEVELOPMENT.md](DEVELOPMENT.md) - Development guidelines
- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture
- [API_EXAMPLES.md](API_EXAMPLES.md) - Code examples
- [DIAGRAMS.md](DIAGRAMS.md) - Architecture diagrams

---

## License

By contributing to LogViewer, you agree that your contributions will be licensed under the same license as the project.

---

## Maintainers

- @bula173 - Project lead
- Maintainers will review all PRs and provide feedback

Thank you for contributing to LogViewer! 🎉
