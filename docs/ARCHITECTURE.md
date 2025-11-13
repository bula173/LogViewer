# LogViewer Architecture

## Overview

LogViewer is a professional log file viewer application built with modern C++20 and wxWidgets, following clean architecture principles and industry-standard design patterns.

## Core Design Principles

1. **Separation of Concerns**: Clear boundaries between GUI, business logic, and data layers
2. **SOLID Principles**: Single responsibility, open/closed, dependency inversion
3. **Observer Pattern**: Loose coupling between parsers and UI
4. **Factory Pattern**: Flexible parser instantiation
5. **Strategy Pattern**: Pluggable filtering algorithms
6. **Command Pattern**: Undo/redo operations

## Architecture Layers

```
┌─────────────────────────────────────────────────────────────────┐
│                         Presentation Layer                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  MainWindow  │  │ FilterPanel  │  │  Dialogs     │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└───────────────────────────┬─────────────────────────────────────┘
                            │ Observer Pattern
┌───────────────────────────┼─────────────────────────────────────┐
│                    Business Logic Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ParserFactory │  │FilterManager │  │    Config    │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│  ┌──────────────────────────────────────────────────┐           │
│  │           Parser Strategies                       │           │
│  │  ┌────────────┐ ┌────────────┐ ┌────────────┐   │           │
│  │  │ XmlParser  │ │ JsonParser │ │ CsvParser  │   │           │
│  │  └────────────┘ └────────────┘ └────────────┘   │           │
│  └──────────────────────────────────────────────────┘           │
└───────────────────────────┬─────────────────────────────────────┘
                            │ Data Access
┌───────────────────────────┼─────────────────────────────────────┐
│                        Data Layer                                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │EventsContainer│ │   LogEvent   │  │    Filter    │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└─────────────────────────────────────────────────────────────────┘
```

## Key Components

### 1. Presentation Layer (`gui` namespace)

**MainWindow**: Central orchestrator
- Manages all UI components
- Coordinates user interactions
- Implements `IDataParserObserver` for parser updates
- Implements `ConfigObserver` for configuration changes

**Virtual List Controls**: High-performance display
- `EventsVirtualListControl`: Main event list (supports millions of entries)
- `ItemVirtualListControl`: Event detail viewer

### 2. Business Logic Layer

**ParserFactory**: Creates appropriate parsers
```cpp
// Usage:
auto parser = ParserFactory::CreateParser(filepath);
parser->RegisterObserver(observer);
parser->ParseData(filepath);
```

**FilterManager**: Coordinates filtering operations
- Applies multiple filters in sequence
- Supports complex filter combinations
- Implements Strategy pattern for filter types

**Config**: Centralized configuration management
- JSON-based configuration
- Observable changes
- Schema validation

### 3. Data Layer (`db` namespace)

**EventsContainer**: High-performance event storage
- O(1) random access
- Thread-safe operations (with mutex)
- Virtual list integration
- Implements `IModel` interface

**LogEvent**: Immutable event representation
- ID + key-value pairs
- Copy-on-write semantics
- Efficient memory layout

## Design Patterns

### Observer Pattern
**Purpose**: Decouple parser from UI updates

```cpp
class IDataParserObserver {
    virtual void ProgressUpdated() = 0;
    virtual void NewEventFound(db::LogEvent&& event) = 0;
    virtual void NewEventBatchFound(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& eventBatch) = 0;
};
```

**Benefits**:
- UI can update without parser knowing implementation details
- Multiple observers can watch same parser
- Easy to add progress bars, status updates, etc.

### Factory Pattern
**Purpose**: Abstract parser creation

```cpp
class ParserFactory {
public:
    static std::unique_ptr<IDataParser> CreateParser(
        const std::filesystem::path& filepath);
    static std::unique_ptr<IDataParser> CreateParser(
        ParserType type);
};
```

**Benefits**:
- Easy to add new parser types
- Client code doesn't depend on concrete parser classes
- Configuration-driven parser selection

### Strategy Pattern
**Purpose**: Pluggable filter matching algorithms

```cpp
class IFilterStrategy {
public:
    virtual ~IFilterStrategy() = default;
    virtual bool matches(const std::string& value,
                        const std::string& pattern) const = 0;
};

class RegexFilterStrategy : public IFilterStrategy { /*...*/ };
class ExactMatchStrategy : public IFilterStrategy { /*...*/ };
class FuzzyMatchStrategy : public IFilterStrategy { /*...*/ };
```

**Benefits**:
- Runtime filter algorithm selection
- Easy to add new filter types
- Testable in isolation

### Command Pattern
**Purpose**: Encapsulate operations for undo/redo

```cpp
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

class ApplyFilterCommand : public ICommand { /*...*/ };
class RemoveFilterCommand : public ICommand { /*...*/ };
```

**Benefits**:
- Undo/redo functionality
- Operation history
- Macro recording

### MVC Pattern
**Purpose**: Separate data, presentation, and control logic

- **Model** (`IModel`, `EventsContainer`): Data and business logic
- **View** (`IView`, `MainWindow`): Presentation and rendering
- **Controller** (`IController`): User input handling

## Thread Safety

### Parsing Thread
- Background thread for file parsing
- Notifies UI thread via observer callbacks
- Uses atomic flags for cancellation

### UI Thread
- All wxWidgets operations on main thread
- Uses `wxQueueEvent` for cross-thread communication
- Mutex-protected shared data structures

```cpp
class EventsContainer {
private:
    mutable std::mutex m_mutex;
    std::vector<LogEvent> m_data;
    
public:
    void AddEvent(LogEvent&& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.push_back(std::move(event));
    }
};
```

## Error Handling Strategy

### Result<T, Error> Pattern
Modern error handling without exceptions in hot paths:

```cpp
template<typename T>
class Result {
public:
    static Result Ok(T value);
    static Result Err(Error error);
    
    bool isOk() const;
    bool isErr() const;
    T unwrap();
    Error error() const;
};

// Usage:
Result<LogEvent, ParseError> ParseEvent(const std::string& line) {
    if (line.empty())
        return Result::Err(ParseError::EmptyLine);
    
    LogEvent event = /*...*/;
    return Result::Ok(std::move(event));
}
```

### Exception Hierarchy
```
std::exception
  └─ error::Error (base for all app exceptions)
       ├─ error::ConfigError
       ├─ error::ParseError
       ├─ error::FileError
       └─ error::FilterError
```

## Performance Optimizations

1. **Virtual Lists**: Only render visible items
2. **Batch Processing**: Process events in batches (1000-10000 at a time)
3. **Move Semantics**: Avoid unnecessary copies
4. **Precompiled Headers**: Reduce compilation time
5. **Smart Pointers**: Automatic memory management
6. **Reserve Capacity**: Pre-allocate vector space
7. **String Interning**: Reuse common strings

## Configuration Management

### JSON Schema
All configuration files are validated against JSON schema:

```json
{
  "version": "1.0",
  "logging": {
    "level": "info",
    "file": "logviewer.log"
  },
  "parsers": {
    "xml": {
      "root_element": "log",
      "event_element": "event"
    }
  },
  "ui": {
    "theme": "dark",
    "max_recent_files": 10
  }
}
```

### Observer Pattern for Config
Components register for configuration changes:

```cpp
class ConfigObserver {
public:
    virtual void OnConfigChanged() = 0;
};

Config::Instance().AddObserver(this);
```

## Testing Strategy

### Unit Tests
- Google Test framework
- Mock observers for testing parsers
- Test fixtures for common scenarios
- 80%+ code coverage goal

### Integration Tests
- Full parser → filter → display pipeline
- Real log file samples
- Performance benchmarks

### UI Tests
- Manual test scenarios
- Screenshot comparison
- Accessibility testing

## Future Enhancements

1. **Plugin System**: Load parsers from DLLs
2. **Scripting**: Lua/Python for custom filters
3. **Network Logs**: Real-time log streaming
4. **Database Export**: Export to SQLite/PostgreSQL
5. **Log Analysis**: Statistics, graphs, anomaly detection
6. **Collaborative Filtering**: Share filter configurations
7. **Cloud Integration**: S3, Azure Blob Storage support

## References

- [wxWidgets Best Practices](https://docs.wxwidgets.org/)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [Design Patterns](https://refactoring.guru/design-patterns)
- [Modern C++ Design](https://www.amazon.com/Modern-Design-Generic-Programming-Patterns/dp/0201704315)
