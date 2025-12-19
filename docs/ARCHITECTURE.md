# LogViewer Architecture

## Overview

LogViewer is a professional log file viewer application built with modern C++20 and Qt 6, following clean architecture principles and industry-standard design patterns. The application provides powerful AI-assisted log analysis, flexible filtering, and a highly customizable dock-based UI.

## Core Design Principles

1. **Separation of Concerns**: Clear boundaries between GUI, business logic, and data layers
2. **SOLID Principles**: Single responsibility, open/closed, dependency inversion
3. **Observer Pattern**: Loose coupling between components and UI
4. **Factory Pattern**: Flexible parser and AI client instantiation
5. **Strategy Pattern**: Pluggable filtering and AI provider algorithms
6. **MVC Pattern**: Model-View-Controller separation for data and presentation

## Architecture Layers

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           Presentation Layer (Qt 6)                       │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐             │
│  │   MainWindow   │  │  Dock Widgets  │  │    Dialogs     │             │
│  │  (QMainWindow) │  │  (QDockWidget) │  │  (QDialog)     │             │
│  └────────────────┘  └────────────────┘  └────────────────┘             │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐             │
│  │EventsTableView │  │ AIAnalysisPanel│  │  FiltersPanel  │             │
│  │(QTableView)    │  │  (QWidget)     │  │   (QWidget)    │             │
│  └────────────────┘  └────────────────┘  └────────────────┘             │
└────────────────────────────┬─────────────────────────────────────────────┘
                             │ Presenter Pattern
┌────────────────────────────┼─────────────────────────────────────────────┐
│                      Business Logic Layer                                 │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │
│  │MainWindowPresent│  │  FilterManager  │  │     Config      │          │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘          │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │
│  │  ParserFactory  │  │ AIServiceFactory│  │  LogAnalyzer    │          │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘          │
│  ┌──────────────────────────────────────────────────────────────┐        │
│  │                    Parser Strategies                          │        │
│  │  ┌────────────┐ ┌────────────┐ ┌────────────┐               │        │
│  │  │ XmlParser  │ │ JsonParser │ │ CsvParser  │               │        │
│  │  └────────────┘ └────────────┘ └────────────┘               │        │
│  └──────────────────────────────────────────────────────────────┘        │
│  ┌──────────────────────────────────────────────────────────────┐        │
│  │                   AI Service Strategies                       │        │
│  │  ┌────────────┐ ┌────────────┐ ┌────────────┐               │        │
│  │  │OllamaClient│ │OpenAIClient│ │AnthropicCl.│               │        │
│  │  └────────────┘ └────────────┘ └────────────┘               │        │
│  └──────────────────────────────────────────────────────────────┘        │
└────────────────────────────┬─────────────────────────────────────────────┘
                             │ Data Access
┌────────────────────────────┼─────────────────────────────────────────────┐
│                          Data Layer                                       │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐          │
│  │EventsContainer  │  │    LogEvent     │  │     Filter      │          │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘          │
└──────────────────────────────────────────────────────────────────────────┘
```

## Key Components

### 1. Presentation Layer (`ui::qt` namespace)

**MainWindow**: Central orchestrator
- Qt 6-based QMainWindow with dock widget system
- Three dockable panels: Filters (left), Item Details (right), Tools (bottom)
- Tab-based main content area (Events, AI Analysis)
- Drag-and-drop file loading support
- Implements `ConfigObserver` for configuration changes
- Menu system for file operations, tools, and view management

**EventsTableView**: High-performance table display
- QTableView with custom EventsTableModel
- Supports millions of entries with virtual scrolling
- Movable and resizable columns
- Multi-selection and copy support
- Color-coded rows based on configurable `typeFilterField`

**AIAnalysisPanel**: AI-powered log analysis
- Predefined analysis types (Summary, Error Analysis, Pattern Detection, etc.)
- Custom prompt support with save/load functionality
- Model selection dropdown
- Filter-aware analysis (respects active filters)
- Configurable max events and smart sampling

**Dock Widgets**: Flexible layout
- **FiltersPanel**: Type filters and extended filters with multiple conditions
- **ItemDetailsView**: Detailed event information display
- **SearchResultsView**: Full-text search results
- **AIChatPanel**: Interactive AI conversation about logs
- All docks are movable, floatable, and resizable

**Configuration Dialogs**:
- **StructuredConfigDialog**: Tabbed configuration UI (General, Columns, Colors, AI)
- **ConfigEditorDialog**: Raw JSON editor
- **OllamaSetupDialog**: AI setup wizard

### 2. Business Logic Layer

**MainWindowPresenter**: MVP pattern implementation
- Coordinates between view and model
- Handles search operations and filter application
- Uses configurable `typeFilterField` for type filtering
- Progress tracking and status updates

**AIServiceFactory**: Creates appropriate AI clients
```cpp
// Usage:
auto aiService = AIServiceFactory::CreateClient(
    "ollama",              // provider
    "",                    // API key (empty for local)
    "http://localhost:11434",  // base URL
    "qwen2.5-coder:7b"    // model
);
```

**LogAnalyzer**: AI analysis orchestration
- Formats events for AI consumption (sends all fields dynamically)
- Implements predefined analysis types
- Supports custom prompts
- Filter-aware: analyzes only filtered events when filters active
- Smart sampling: caps at 5,000 events with even distribution

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
- JSON-based configuration with validation
- Observable changes via ConfigObserver pattern
- Configurable fields:
  - `typeFilterField`: Field used for type filtering/coloring (e.g., "level", "severity")
  - `aiProvider`: AI service provider selection
  - `aiTimeoutSeconds`: Configurable timeout for AI requests (30-3600s)
  - Column configurations, colors, logging level

### 3. Data Layer (`db` namespace)

**EventsContainer**: High-performance event storage
- O(1) random access
- Thread-safe operations (with mutex)
- Supports filter indices for efficient filtered views
- Implements `IModel` interface
- Observable updates for UI refresh

**LogEvent**: Immutable event representation
- ID + key-value pairs (flexible schema)
- `findByKey()` method for dynamic field access
- Efficient memory layout
- Move semantics for performance

## Design Patterns

### Factory Pattern
**Purpose**: Abstract creation of parsers and AI clients

**ParserFactory**: Creates appropriate parser based on file extension
```cpp
class ParserFactory {
public:
    static std::unique_ptr<IDataParser> CreateParser(
        const std::filesystem::path& filepath);
    static std::unique_ptr<IDataParser> CreateParser(
        ParserType type);
};
```

**AIServiceFactory**: Creates appropriate AI client based on provider
```cpp
class AIServiceFactory {
public:
    static std::shared_ptr<IAIService> CreateClient(
        const std::string& provider,
        const std::string& apiKey,
        const std::string& baseUrl,
        const std::string& model);
    
    static bool RequiresApiKey(const std::string& provider);
    static std::string GetDefaultBaseUrl(const std::string& provider);
    static std::string GetDefaultModel(const std::string& provider);
};
```

**Benefits**:
- Easy to add new parser types and AI providers
- Client code doesn't depend on concrete classes
- Configuration-driven selection

### Strategy Pattern
**Purpose**: Pluggable algorithms for filtering and AI providers

**Filter Strategies**:
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

**AI Service Interface**:
```cpp
class IAIService {
public:
    virtual ~IAIService() = default;
    virtual std::string SendPrompt(const std::string& prompt) = 0;
    virtual bool IsAvailable() const = 0;
    virtual std::string GetModelName() const = 0;
};

class OllamaClient : public IAIService { /*...*/ };
class OpenAIClient : public IAIService { /*...*/ };
class AnthropicClient : public IAIService { /*...*/ };
class GeminiClient : public IAIService { /*...*/ };
```

**Benefits**:
- Runtime algorithm selection (filter types, AI providers)
- Easy to add new strategies
- Testable in isolation
- No code changes needed to switch providers

### Observer Pattern
**Purpose**: Decouple components and enable reactive UI

**ConfigObserver**: React to configuration changes
```cpp
class ConfigObserver {
public:
    virtual void OnConfigChanged() = 0;
};

// MainWindow implements ConfigObserver
void MainWindow::OnConfigChanged() {
    // Refresh views with new configuration
    m_eventsView->UpdateColors();
    m_eventsView->RefreshView();
}
```

**Benefits**:
- Loose coupling between configuration and UI
- Multiple components can observe same config
- Easy to add new observers

### MVC/MVP Pattern
**Purpose**: Separate data, presentation, and control logic

- **Model** (`EventsContainer`, `LogEvent`): Data and business logic
- **View** (`MainWindow`, `EventsTableView`, dock widgets): Presentation and rendering  
- **Presenter** (`MainWindowPresenter`): Coordinates between view and model, handles user actions

```cpp
class MainWindowPresenter {
    IMainWindowView& m_view;
    IController& m_controller;
    EventsContainer& m_events;
    
public:
    void PerformSearch();
    void LoadLogFile(const std::filesystem::path& path);
    void ApplySelectedTypeFilters();
    void UpdateTypeFilters();
};
```

**Benefits**:
- Clear separation of concerns
- Testable business logic (presenter can be unit tested)
- View can be replaced (Qt, wxWidgets, CLI, web)

## AI Integration Architecture

### Component Overview
```
┌──────────────────┐
│  AIAnalysisPanel │ (UI)
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│   LogAnalyzer    │ (Business Logic)
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  IAIService      │ (Interface)
└────────┬─────────┘
         │
    ┌────┴─────┬──────────┬───────────┬─────────┐
    ▼          ▼          ▼           ▼         ▼
┌─────────┐┌─────────┐┌──────────┐┌─────────┐┌─────────┐
│Ollama   ││OpenAI   ││Anthropic ││Gemini   ││Custom   │
│Client   ││Client   ││Client    ││Client   ││Client   │
└─────────┘└─────────┘└──────────┘└─────────┘└─────────┘
```

### AI Request Flow
1. User selects analysis type in AIAnalysisPanel
2. Panel calls LogAnalyzer with parameters
3. LogAnalyzer formats events (all fields, filter-aware)
4. LogAnalyzer builds prompt based on analysis type
5. Prompt sent to IAIService implementation
6. Response parsed and displayed in UI

### Filter-Aware Analysis
- **No filters**: Analyzes all events (up to 5,000 with smart sampling)
- **With filters**: Analyzes only filtered events
- EventsTableView provides `GetFilteredIndices()` method
- LogAnalyzer accepts optional `filteredIndices` parameter

### Smart Sampling
For large datasets:
- Cap at 5,000 events (configurable)
- Even distribution across timeline
- Preserves representative sample
- Avoids overwhelming LLM context window

## Thread Safety

### Qt Event Loop
- All UI operations on main thread
- Qt's signal/slot mechanism for cross-thread communication
- `QtConcurrent` for AI requests in background thread

### Parsing Thread
- Background thread for file parsing (future enhancement)
- Currently synchronous on main thread
- Planned: Async parsing with progress callbacks

### Concurrent AI Requests
```cpp
// AI analysis runs in background thread via QtConcurrent
QFuture<std::string> future = QtConcurrent::run([this, prompt]() {
    return m_aiService->SendPrompt(prompt);
});

auto* watcher = new QFutureWatcher<std::string>(this);
connect(watcher, &QFutureWatcher<std::string>::finished,
    this, &AIAnalysisPanel::OnAnalysisComplete);
watcher->setFuture(future);
```

### Data Access
```cpp
class EventsContainer {
private:
    mutable std::mutex m_mutex;
    std::vector<LogEvent> m_data;
    std::vector<unsigned long> m_filteredIndices;
    
public:
    void AddEvent(LogEvent&& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.push_back(std::move(event));
    }
    
    const std::vector<unsigned long>& GetFilteredIndices() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_filteredIndices;
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

1. **Qt Model/View Architecture**: Only render visible items in table
2. **Batch Processing**: Process events in batches during parsing
3. **Move Semantics**: Avoid unnecessary copies with std::move
4. **Smart Pointers**: Automatic memory management (shared_ptr, unique_ptr)
5. **Reserve Capacity**: Pre-allocate vector space for known sizes
6. **Filter Indices**: Store filtered indices separately, avoid copying events
7. **Smart Sampling**: Cap AI analysis at 5,000 events with even distribution
8. **Configurable Timeout**: Prevent timeouts on slow machines (30-3600s)
9. **Dynamic Field Access**: `findByKey()` for flexible event schemas
10. **Color Caching**: Qt's QBrush caching for color rendering

## Configuration Management

### JSON Configuration
All configuration saved to platform-specific location:
- **macOS**: `~/Library/Application Support/LogViewer/config.json`
- **Linux**: `~/.config/LogViewer/config.json`
- **Windows**: `%APPDATA%\LogViewer\config.json`

### Configuration Structure
```json
{
  "version": "1.0",
  "logging": {
    "level": "debug"
  },
  "filters": {
    "typeFilterField": "level"
  },
  "aiConfig": {
    "provider": "ollama",
    "baseUrl": "http://localhost:11434",
    "defaultModel": "qwen2.5-coder:7b",
    "apiKey": "",
    "timeoutSeconds": 300
  },
  "parsers": {
    "xml": {
      "rootElement": "events",
      "eventElement": "event",
      "columns": [
        { "name": "id", "visible": true, "width": 50 },
        { "name": "timestamp", "visible": true, "width": 150 },
        { "name": "level", "visible": true, "width": 200 },
        { "name": "info", "visible": true, "width": 300 }
      ]
    }
  },
  "columnColors": {
    "level": {
      "ERROR": ["#ffffff", "#ff4200"],
      "WARN": ["#000000", "#ffcc00"],
      "INFO": ["#000000", "#90ee90"],
      "DEBUG": ["#000000", "#d3d3d3"]
    }
  }
}
```

### Key Configuration Options
- **typeFilterField**: Configurable field for type filtering (e.g., "type", "level", "severity")
- **aiProvider**: AI service selection (ollama, lmstudio, openai, anthropic, google, xai)
- **aiTimeoutSeconds**: Timeout for AI requests (30-3600 seconds)
- **columns**: Dynamic column configuration with visibility and width
- **columnColors**: Color mappings based on field values

### Observer Pattern for Config
Components register for configuration changes:

```cpp
class ConfigObserver {
public:
    virtual void OnConfigChanged() = 0;
};

// MainWindow refreshes UI when config changes
void MainWindow::OnConfigChanged() {
    if (m_eventsView) {
        m_eventsView->UpdateColors();
        m_eventsView->RefreshView();
    }
}
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

1. **Async Parsing**: Background thread for file parsing with progress
2. **Plugin System**: Load parsers and AI clients from DLLs
3. **Network Logs**: Real-time log streaming via TCP/HTTP
4. **Database Export**: Export to SQLite/PostgreSQL
5. **Advanced Analytics**: Statistics, graphs, anomaly detection
6. **Collaborative Filtering**: Share filter configurations
7. **Cloud Integration**: S3, Azure Blob Storage, Google Cloud Storage
8. **Custom AI Prompts Library**: Community-shared analysis templates
9. **Log Diff**: Compare two log files side-by-side
10. **Session Replay**: Record and replay user analysis workflows
11. **Multi-File Analysis**: Correlate events across multiple log files
12. **Real-time AI Monitoring**: Continuous analysis with alerts

## References

- [Qt Documentation](https://doc.qt.io/)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [Design Patterns](https://refactoring.guru/design-patterns)
- [Modern C++ Design](https://www.amazon.com/Modern-Design-Generic-Programming-Patterns/dp/0201704315)
- [Ollama Documentation](https://ollama.ai/docs)
- [OpenAI API Reference](https://platform.openai.com/docs)
- [Anthropic Claude API](https://docs.anthropic.com/)
