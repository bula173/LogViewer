# LogViewer Development Guide

## Getting Started

### Prerequisites
- **CMake** 3.25+
- **C++20 Compiler**: Clang 15+, GCC 11+, or MSVC 2019+
- **wxWidgets** 3.2+
- **Ninja** build system (recommended)
- **ccache** (optional, for faster rebuilds)

### Quick Start

```bash
# Clone repository
git clone https://github.com/bula173/LogViewer.git
cd LogViewer

# Configure and build (Debug)
cmake --preset windows-msys-debug  # or linux-debug, macos-debug
cmake --build --preset windows-msys-debug-build

# Run tests
ctest --preset windows-msys-debug-test

# Run application
./dist/Debug/LogViewer
```

## Code Style Guide

### Naming Conventions

```cpp
// Classes: PascalCase
class EventsContainer { };

// Functions/Methods: camelCase
void parseData();
int getCurrentIndex();

// Member variables: m_camelCase
private:
    std::string m_fileName;
    int m_eventCount;

// Constants: UPPER_SNAKE_CASE
const int MAX_EVENTS = 1000000;

// Namespaces: lowercase
namespace parser { }
namespace filters { }

// Template parameters: PascalCase
template<typename T, typename ErrorType>
class Result { };
```

### File Organization

```cpp
// Header file: EventsContainer.hpp
/**
 * @file EventsContainer.hpp
 * @brief Brief description
 * @author Your Name
 * @date 2025
 */

#pragma once

// System includes
#include <vector>
#include <memory>

// Third-party includes
#include <nlohmann/json.hpp>

// Project includes
#include "db/LogEvent.hpp"
#include "mvc/IModel.hpp"

namespace db {

/**
 * @class EventsContainer
 * @brief Detailed description
 *
 * More details about the class, usage examples, performance characteristics.
 */
class EventsContainer {
public:
    // Constructors
    EventsContainer();
    explicit EventsContainer(size_t reserveSize);
    
    // Destructor
    virtual ~EventsContainer();
    
    // Copy/Move (Rule of 5)
    EventsContainer(const EventsContainer&) = delete;
    EventsContainer& operator=(const EventsContainer&) = delete;
    EventsContainer(EventsContainer&&) noexcept = default;
    EventsContainer& operator=(EventsContainer&&) noexcept = default;
    
    // Public interface
    void AddEvent(LogEvent&& event);
    const LogEvent& GetEvent(int index) const;
    size_t Size() const;
    
private:
    // Private methods
    void validateIndex(int index) const;
    
    // Member variables
    std::vector<LogEvent> m_data;
    mutable std::mutex m_mutex;
};

} // namespace db
```

### Documentation Standards

#### Doxygen Comments

```cpp
/**
 * @brief Adds a new event to the container.
 *
 * Appends the event to the internal storage using move semantics
 * for efficiency. The method is thread-safe and can be called
 * from multiple threads concurrently.
 *
 * @param event The LogEvent to add (moved into container)
 *
 * @throws std::bad_alloc if memory allocation fails
 *
 * @note The event is moved, so the passed object will be in a
 *       valid but unspecified state after this call.
 *
 * @complexity O(1) amortized
 *
 * @par Example:
 * @code
 * EventsContainer container;
 * LogEvent event(1, {{"message", "Error occurred"}});
 * container.AddEvent(std::move(event));
 * // event is no longer valid here
 * @endcode
 *
 * @see GetEvent(), Size()
 */
void AddEvent(LogEvent&& event);
```

### Code Quality Checklist

#### Before Committing
- [ ] All tests pass
- [ ] No compiler warnings
- [ ] Code formatted with clang-format
- [ ] Doxygen comments for public APIs
- [ ] Thread safety documented
- [ ] Performance implications noted
- [ ] Error handling implemented
- [ ] Resource cleanup (RAII)

## Design Patterns Usage

### When to Use Each Pattern

#### Observer Pattern ✅
**Use when**: Components need loose coupling for event notification

```cpp
// Good: Parser doesn't know about UI
class XmlParser : public IDataParser {
    void ParseData(const std::filesystem::path& filepath) override {
        // ... parsing logic ...
        NotifyNewEvent(std::move(event));  // Observers updated
    }
};
```

#### Factory Pattern ✅
**Use when**: Object creation logic is complex or varies

```cpp
// Good: Centralized parser creation
auto parser = ParserFactory::CreateParser(".xml");  // Returns XmlParser
auto parser = ParserFactory::CreateParser(".json"); // Returns JsonParser
```

#### Strategy Pattern ✅
**Use when**: Algorithm needs to be swappable at runtime

```cpp
// Good: Different filter strategies
filter.SetStrategy(std::make_unique<RegexStrategy>());
filter.SetStrategy(std::make_unique<ExactMatchStrategy>());
```

#### Singleton Pattern ⚠️
**Use sparingly**: Only for truly global resources

```cpp
// Good use: Application configuration
Config& config = Config::Instance();

// Bad use: Don't make everything a singleton!
// Use dependency injection instead
```

## Error Handling

### Error Categories

```cpp
namespace error {

// Base class for all application errors
class Error : public std::exception {
public:
    explicit Error(std::string message) : m_message(std::move(message)) {}
    const char* what() const noexcept override { return m_message.c_str(); }
    
private:
    std::string m_message;
};

// Specific error types
class ConfigError : public Error {
    using Error::Error;
};

class ParseError : public Error {
    using Error::Error;
};

class FileError : public Error {
    using Error::Error;
};

} // namespace error
```

### Error Handling Guidelines

```cpp
// ❌ Bad: Catching everything
try {
    DoSomething();
} catch (...) {
    // What went wrong? Can't tell!
}

// ✅ Good: Specific error handling
try {
    config.Load("config.json");
} catch (const error::ConfigError& e) {
    spdlog::error("Configuration error: {}", e.what());
    // Provide default configuration
    config.UseDefaults();
} catch (const error::FileError& e) {
    spdlog::error("File error: {}", e.what());
    // Show file picker dialog
    ShowFilePickerDialog();
}

// ✅ Best: Result<T> for expected failures
Result<Config, ConfigError> LoadConfig(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return Result::Err(ConfigError("File not found"));
    }
    
    Config config = /* ... */;
    return Result::Ok(std::move(config));
}

// Usage
auto result = LoadConfig("config.json");
if (result.isOk()) {
    Config config = result.unwrap();
    // use config
} else {
    spdlog::error("Failed to load config: {}", result.error().what());
}
```

## Thread Safety

### Guidelines

1. **Document thread safety** of every class
2. **Prefer immutability** when possible
3. **Use RAII** for lock management
4. **Avoid deadlocks** with lock ordering

```cpp
/**
 * @class EventsContainer
 * @brief Thread-safe container for log events
 *
 * @par Thread Safety
 * All methods are thread-safe. Multiple threads can add events
 * concurrently. Read operations are lock-free for const methods.
 */
class EventsContainer {
public:
    /// Thread-safe addition (locks internally)
    void AddEvent(LogEvent&& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.push_back(std::move(event));
    }
    
    /// Thread-safe read (shared lock)
    const LogEvent& GetEvent(int index) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_data.at(index);
    }
    
    /// Lock-free size query
    size_t Size() const noexcept {
        return m_data.size();  // atomic on most platforms
    }
    
private:
    mutable std::shared_mutex m_mutex;
    std::vector<LogEvent> m_data;
};
```

## Performance Best Practices

### Memory Management

```cpp
// ❌ Bad: Unnecessary copies
void ProcessEvents(EventsContainer container) {
    // Entire container copied!
}

// ✅ Good: Pass by const reference
void ProcessEvents(const EventsContainer& container) {
    // No copy, read-only access
}

// ✅ Better: Use move semantics when transferring ownership
void TakeOwnership(EventsContainer&& container) {
    m_container = std::move(container);
}
```

### String Handling

```cpp
// ❌ Bad: String concatenation in loop
std::string result;
for (const auto& item : items) {
    result += item + ", ";  // Multiple reallocations!
}

// ✅ Good: Reserve capacity
std::string result;
result.reserve(estimatedSize);
for (const auto& item : items) {
    result += item + ", ";
}

// ✅ Best: Use string stream for complex formatting
std::ostringstream oss;
for (const auto& item : items) {
    oss << item << ", ";
}
std::string result = oss.str();
```

### Container Operations

```cpp
// ❌ Bad: Reallocations during growth
std::vector<LogEvent> events;
for (int i = 0; i < 1000000; ++i) {
    events.push_back(CreateEvent(i));  // Many reallocations!
}

// ✅ Good: Reserve capacity upfront
std::vector<LogEvent> events;
events.reserve(1000000);
for (int i = 0; i < 1000000; ++i) {
    events.push_back(CreateEvent(i));
}

// ✅ Best: Use emplace_back to construct in-place
events.reserve(1000000);
for (int i = 0; i < 1000000; ++i) {
    events.emplace_back(i, CreateItems(i));
}
```

## Testing

### Unit Test Structure

```cpp
#include <gtest/gtest.h>
#include "db/EventsContainer.hpp"

namespace db::test {

// Test fixture for common setup
class EventsContainerTest : public ::testing::Test {
protected:
    void SetUp() override {
        container = std::make_unique<EventsContainer>();
    }
    
    void TearDown() override {
        container.reset();
    }
    
    std::unique_ptr<EventsContainer> container;
};

// Individual tests
TEST_F(EventsContainerTest, AddSingleEvent) {
    LogEvent event(1, {{"key", "value"}});
    container->AddEvent(std::move(event));
    
    EXPECT_EQ(container->Size(), 1);
    EXPECT_EQ(container->GetEvent(0).getId(), 1);
}

TEST_F(EventsContainerTest, ThrowsOnInvalidIndex) {
    EXPECT_THROW(container->GetEvent(0), std::out_of_range);
}

TEST_F(EventsContainerTest, MultiThreadedAddition) {
    constexpr int NUM_THREADS = 10;
    constexpr int EVENTS_PER_THREAD = 1000;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < EVENTS_PER_THREAD; ++i) {
                LogEvent event(t * EVENTS_PER_THREAD + i, {});
                container->AddEvent(std::move(event));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(container->Size(), NUM_THREADS * EVENTS_PER_THREAD);
}

} // namespace db::test
```

### Mock Objects

```cpp
// Mock observer for testing parsers
class MockObserver : public IDataParserObserver {
public:
    MOCK_METHOD(void, ProgressUpdated, (), (override));
    MOCK_METHOD(void, NewEventFound, (LogEvent&&), (override));
    MOCK_METHOD(void, NewEventBatchFound,
        (std::vector<std::pair<int, LogEvent::EventItems>>&&), (override));
};

// Usage in tests
TEST(XmlParserTest, NotifiesObservers) {
    MockObserver observer;
    XmlParser parser;
    parser.RegisterObserver(&observer);
    
    EXPECT_CALL(observer, NewEventFound(testing::_))
        .Times(testing::AtLeast(1));
    
    parser.ParseData("test.xml");
}
```

## Debugging Tips

### Logging Levels

```cpp
// Use appropriate log levels
spdlog::trace("Entering function with param={}", param);  // Very verbose
spdlog::debug("Processing event id={}", id);              // Development
spdlog::info("File loaded: {}", filename);                // Normal operation
spdlog::warn("Config missing, using defaults");           // Potential issues
spdlog::error("Failed to parse: {}", error);              // Errors
spdlog::critical("Out of memory!");                       // Fatal errors
```

### Common Issues

**Issue**: Crash on vector access
```cpp
// Debug build: Check assertions
assert(index < m_data.size() && "Index out of range");

// Release build: Use at() for bounds checking
return m_data.at(index);  // Throws std::out_of_range
```

**Issue**: Memory leaks
```cpp
// Use smart pointers!
std::unique_ptr<Parser> parser = ParserFactory::CreateParser(".xml");
// Automatically deleted when out of scope

// Or shared ownership
std::shared_ptr<Config> config = std::make_shared<Config>();
```

**Issue**: Deadlocks
```cpp
// Always acquire locks in same order
class A {
    void DoWork() {
        std::lock_guard<std::mutex> lock1(m_mutex);
        std::lock_guard<std::mutex> lock2(other.m_mutex);  // ❌ Can deadlock!
    }
};

// Solution: Use std::scoped_lock for multiple mutexes
void DoWork() {
    std::scoped_lock lock(m_mutex, other.m_mutex);  // ✅ Deadlock-free
}
```

## Git Workflow

### Branch Naming
- `feature/add-json-parser` - New features
- `bugfix/fix-crash-on-empty-file` - Bug fixes
- `refactor/improve-filter-performance` - Code improvements
- `docs/update-architecture` - Documentation

### Commit Messages
```
feat: Add JSON parser support

- Implement JsonParser class following IDataParser interface
- Add tests for JSON parsing edge cases
- Update ParserFactory to handle .json files

Closes #123
```

### Pull Request Template
```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
- [ ] Unit tests added
- [ ] Integration tests pass
- [ ] Manual testing performed

## Checklist
- [ ] Code follows style guide
- [ ] Documentation updated
- [ ] No new warnings
- [ ] Tests pass locally
```

## Useful Commands

```bash
# Format all code
find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Run static analysis
clang-tidy src/**/*.cpp -- -I src

# Generate documentation
doxygen Doxyfile

# Run with sanitizers
cmake --preset windows-msys-debug -DENABLE_SANITIZER=address
cmake --build --preset windows-msys-debug-build
./dist/Debug/LogViewer

# Profile performance
perf record ./dist/Release/LogViewer
perf report

# Memory profiling
valgrind --tool=massif ./dist/Debug/LogViewer
```

## Resources

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [Effective Modern C++](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/)
- [wxWidgets Documentation](https://docs.wxwidgets.org/)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [CMake Documentation](https://cmake.org/documentation/)
