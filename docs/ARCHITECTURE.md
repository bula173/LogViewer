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
│  │  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌──────────┐ │        │
│  │  │ XmlParser  │ │ JsonParser │ │ CsvParser  │ │AscParser │ │        │
│  │  └────────────┘ └────────────┘ └────────────┘ └──────────┘ │        │
│  │  ┌────────────┐ ┌───────────────────────────┐               │        │
│  │  │ DltParser  │ │       EvlogParser          │               │        │
│  │  └────────────┘ │ + EvlogTemplateRegistry    │               │        │
│  │                 └───────────────────────────┘               │        │
│  │                                            ┌─────────────┐  │        │
│  │                                            │  DbcParser  │  │        │
│  │                                            │ + CanDecoder│  │        │
│  │                                            └─────────────┘  │        │
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

The Qt UI layer is structured into four subdirectories under `src/application/ui/qt/`:

| Subdirectory | Contents |
|---|---|
| `panels/` | All dock and content panels (FiltersPanel, StatsSummaryPanel, SignalPlotPanel, TimelineChartPanel, TraceViewerPanel, BookmarksPanel, ScenariosPanel, ActorsPanel, PatternAnalysisPanel, SideBySidePanel, …) |
| `dialogs/` | Modal dialogs (ConfigEditorDialog, FilterEditorDialog, LogFileLoadDialog, UpdateDialog, …) |
| `events/` | Event table model and view (EventsTableModel, EventsTableView) |
| `utils/` | Shared utilities (PanelUtils, ThemeSwitcher, ExportManager, TypeFilterView, UpdateChecker) |

**MainWindow**: Central orchestrator
- Qt 6-based QMainWindow with dock widget system
- Left docks: **Filters** + **Signal Browser** (tabbed, `QDockWidget` each); right dock: Details; bottom dock: AI chat / plugin panels
- Tab-based content area: Events, Statistics, Signal Plot, Timeline, Pattern Analysis, Trace Viewer, Bookmarks, Scenarios, Actors
- Drag-and-drop file loading; lazy dirty-flag panel refresh (panels only recompute when visible)
- Implements `ConfigObserver` and `IPluginObserver` for configuration and plugin lifecycle changes
- Menu system: File (Open, Load DBC, Load Evlog Templates, Export, …), View (Tabs, Layouts, …), Tools, Help
- `CreateParserFor()` produces format-specific pre-configured parsers: `AscParser` with DBC path for `.asc`; `EvlogParser` with template directory for `.evl`; `ParserFactory` for all other types

**EventsTableView**: High-performance table display (`events/`)
- QTableView with custom EventsTableModel
- Supports millions of entries with virtual scrolling
- Movable and resizable columns; color-coded rows based on configurable `typeFilterField`

**Analysis Panels** (`panels/`)
- **StatsSummaryPanel**: Aggregate statistics with pluggable `IStatisticsStrategy` (see below)
- **SignalPlotPanel**: Plots decoded `SIG:*` field values over time; QLineSeries per signal; downsamples to 2 000 points. Signal selection is delegated to `CanSignalTreePanel` — the plot has no embedded tree.
- **CanSignalTreePanel**: Left-dock Signal Browser; shows DBC frame/signal tree with tristate checkboxes. Populated by `SetDatabase(DbcDatabase)` when a DBC is loaded. Emits `SignalSelectionChanged` → wired to `SignalPlotPanel::SetSelectedSignals()`.
- **LayoutManager**: Serialises and restores named dock layouts. Built-in presets (XML log, CAN analysis) plus user-saved layouts; persisted across sessions.
- **TimelineChartPanel**: Interactive event-volume bar histogram with zoom and brush
- **TraceViewerPanel**: Sequence-diagram-style actor/event view
- **PatternAnalysisPanel**: Template clustering of structurally similar log lines
- **BookmarksPanel**, **ScenariosPanel**, **ActorsPanel**, **ActorDefinitionsPanel**: Event annotation, scenario matching, and actor attribution
- **SideBySidePanel**: Loads two independent log files and displays them in a split view with three synchronisation modes (see below)

**Configuration Dialogs** (`dialogs/`)
- **StructuredConfigDialog**: Tabbed configuration UI (General, Columns, Colors, AI)
- **ConfigEditorDialog**: Raw JSON editor

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

Registered parsers (extension → class):

| Extension | Parser | Notes |
|---|---|---|
| `.xml` | `XmlParser` | Default for unknown extensions |
| `.csv` | `CsvParser` | |
| `.asc` | `AscParser` | CAN/Vector CANalyzer; constructed with optional DBC path via `CreateParserFor()` |
| `.dlt` | `DltParser` | AUTOSAR Diagnostic Log and Trace binary |
| `.evl` | `EvlogParser` | POSIX 1003.25 evlog binary; constructed with optional template directory via `CreateParserFor()` |

**AscParser** (`parsers/asc/`): Parses Vector CANalyzer `.asc` files line by line.
- Each CAN frame becomes a `LogEvent` with fields: `timestamp` (float seconds), `type` (Rx/Tx/TxRq/ErrorFrame), `CAN_ID` (hex), `CAN_Channel`, `CAN_DLC`, `CAN_Data` (hex bytes), `CAN_IDE` (Standard/Extended), `info`.
- If a DBC path is provided, `CanDecoder` is invoked per-frame to append `SIG:<name>` fields for every decoded signal.

**DbcParser + CanDecoder** (`parsers/dbc/`): Reads `.dbc` CAN database files.
- `ParseDbcFile()` returns a `DbcDatabase` containing a map of message ID → `DbcMessage` (name, signals).
- `DecodeFrame()` applies Intel (little-endian) or Motorola (big-endian) bit extraction to produce `{"SIG:<name>", "<value>"}` pairs.

**DltParser** (`parsers/dlt/`): Parses AUTOSAR DLT binary files (`.dlt`).
- Detects optional storage header via `DLT\x01` magic (16-byte: magic + timestamp + ECU ID).
- Reads standard header (HTYP flags control presence of ECU ID, session ID, timestamp), extended header (MSIN → verbose flag, message type, log level; AppID; ContextID).
- Verbose payloads: iterates type-info words and decodes each argument (BOOL, SINT, UINT, FLOA, STRG, RAWD); skips VARI variable-info prefix when present.
- Non-verbose payloads: emits `MsgID=0x…` + hex dump.
- Emitted fields: `timestamp`, `level` (Off/Fatal/Error/Warn/Info/Debug/Verbose), `type` (Log/AppTrace/NwTrace/Control), `AppID`, `ContextID`, `EcuID`, `MsgCtr`, optional `SessionID`, `info`.

**EvlogParser** (`parsers/evlog/`): Parses POSIX 1003.25 evlog binary files (`.evl`).
- Fixed 60-byte little-endian `posix_log_entry` header at known byte offsets; no file-level magic (sanity-checked via severity ≤ 7 and format ∈ {0,1,2,3}).
- Payload formats: `NODATA` (no payload), `STRING` (null-terminated UTF-8), `PRINTF` (format string + binary varargs shown as hex), `BINARY` (template-decoded or hex dump).
- Emitted fields: `timestamp` (float seconds), `level` (Emergency→Debug), `facility` (kern/user/…/local7), `event_type` (hex), `pid`, `uid`, `recid`, optional `cpu` (if ≠ 0), `flags` (comma-separated EVL_* names if set), `info`.
- Call `SetTemplateDirectory(path)` or `SetTemplateFile(path)` before `ParseData()` to enable template-based BINARY decoding.

**EvlogTemplateRegistry** (`parsers/evlog/`): Loads and indexes evlog template files.
- Scans a directory for `.t`, `.tmpl`, `.template` files; parses each with a state-machine line reader.
- Template format: `facility`, `event_type`, `description`, `attributes { typed-fields }`, `format "…%name%…"` keywords (also supports `%keyword%` evlog-native form; `#`/`//` comments; `---` record separator).
- Supports field types: int8–int64, uint8–uint64, float, double, fixed-size char arrays `char[N]`, variable `string`.
- `Find(facility, eventType)` returns the matching `EvlogTemplate*` or `nullptr`; lookup is O(log n) via `std::map`.

**SideBySidePanel** (`panels/`): Displays two independently-loaded log files in a vertical splitter with a shared toolbar for synchronisation control.

- Three sync modes: **Timestamp** (nearest-timestamp scroll), **Manual** (user picks a reference event on each side; a constant offset `rightTs = leftTs + offset` is computed and used for all subsequent lookups), **None** (views independent).
- Each side owns an `EventsContainer` and `EventsTableView`; file loading runs on a background thread via `QtConcurrent::run` and `QFutureWatcher<void>`.
- **Adapter pattern**: the inner `LoadJob::Observer` struct adapts `IDataParserObserver` callbacks to `EventsContainer::AddEvent()`/`AddEventBatch()` without the panel needing to implement the observer interface itself.
- `SuspendNotifications()`/`ResumeNotifications()` are called around bulk loads to suppress per-batch UI redraws.
- Re-entrant sync is guarded by an `m_syncInProgress` flag that prevents left-select → scroll right → right-select → scroll left loops.
- Timestamp-free logs (no `timestamp` field) fall back to a row-offset arithmetic approximation based on the proportional position of the reference row.
- **Async filter deduplication**: `MainWindow::RunAsyncFilter(worker, statusMsg)` encapsulates the `QFutureWatcher` setup, progress bar, and in-progress guard so `ApplyExtendedFilters()` and `ApplyActorFilter()` share a single implementation.

**Statistics Strategy** (`ui/qt/panels/`): The `StatsSummaryPanel` uses `IStatisticsStrategy` to compute format-specific metrics at refresh time without coupling the panel to any particular format.

```
IStatisticsStrategy  ◄────  StatsSummaryPanel::SelectStrategy()
    ▲
    ├── CanStatisticsStrategy   — for ASC/CAN data (detects CAN_ID field)
    └── GenericStatisticsStrategy — fallback, returns no sections (panel hides extra group)
```

`SelectStrategy()` probes the first 20 events for `CAN_ID` to pick the right implementation. Adding a new format requires only implementing `IStatisticsStrategy::Matches()` and `Compute()`.

**FilterManager**: Coordinates filtering operations
- Applies multiple filters in sequence
- Supports complex filter combinations
- Implements Strategy pattern for filter types

**Config**: Centralized configuration management
- JSON-based configuration with validation
- Observable changes via ConfigObserver pattern
- Configurable fields:
  - `typeFilterField`: Field used for type filtering/coloring — auto-detected on each file load; leave empty (default) to auto-detect, or set explicitly to pin a value for all files
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
│  AIAnalysisPanel │ (UI - Qt Widget)
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
         ▼
┌──────────────────────────────────────────────────────────┐
│              AI Provider Plugin (C-ABI)                   │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Plugin_CreateAIService(handle, settingsJson)     │   │
│  └────────────────────┬─────────────────────────────┘   │
│                       │                                   │
│      ┌────────────────┴──────────────────┐              │
│      ▼                ▼                   ▼              │
│  ┌─────────┐    ┌─────────┐        ┌─────────┐         │
│  │Ollama   │    │OpenAI   │        │Anthropic│         │
│  │Client   │    │Client   │        │Client   │         │
│  └─────────┘    └─────────┘        └─────────┘         │
│      ▼                ▼                   ▼              │
│  ┌─────────┐    ┌─────────┐        ┌─────────┐         │
│  │Gemini   │    │xAI Grok │        │LM Studio│         │
│  │Client   │    │Client   │        │Client   │         │
│  └─────────┘    └─────────┘        └─────────┘         │
└──────────────────────────────────────────────────────────┘
```

### Plugin Architecture

AI providers are implemented as C-ABI plugins (see [PLUGIN_SYSTEM.md](PLUGIN_SYSTEM.md)):

**Plugin Exports:**
- `Plugin_Create()` - Creates plugin instance
- `Plugin_SetLoggerCallback(handle, logFn)` - Receives logging callback
- `Plugin_CreateAIService(handle, settings)` - Creates AI service
- `Plugin_CreateMainPanel(handle, parent, settings)` - Analysis panel
- `Plugin_CreateLeftPanel(handle, parent, settings)` - Configuration panel
- `Plugin_CreateBottomPanel(handle, parent, settings)` - Chat panel

**Benefits:**
- Hot-swappable AI providers without recompilation
- Third-party provider support
- ABI-stable interface across compiler versions
- Isolated plugin failures don't crash application

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
    "typeFilterField": ""
  },
  "parsers": {
    "xml": {
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

> **Note on XML parsing**: The XML parser uses element depth to discover structure automatically — no `rootElement` or `eventElement` configuration is needed. The first element at depth 1 is treated as the root; every direct child (depth 2) is an event record; nested children (depth 3+) become key/value fields.

### Key Configuration Options
- **typeFilterField**: Auto-detected on each file load by scanning the first 100 events for common field names (`level`, `type`, `severity`, `priority`, `category`, …). Set explicitly (in Settings > General) to pin a value and skip auto-detection. Leave empty to always auto-detect; if detection fails the user is prompted.
- **aiProvider**: AI service selection (ollama, lmstudio, openai, anthropic, google, xai)
- **aiTimeoutSeconds**: Timeout for AI requests (30-3600 seconds)
- **columns**: Dynamic column configuration with visibility and width
- **columnColors**: Color mappings per column — any number of columns can have color rules; the first matching column's color is applied to the row

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

1. **Extended Plugin System**: Parser plugins, filter plugins, visualization plugins
2. **Network Logs**: Real-time log streaming via TCP/HTTP
3. **Database Export**: Export to SQLite/PostgreSQL
4. **Advanced Analytics**: Anomaly detection, trend analysis
5. **Collaborative Filtering**: Share filter configurations
6. **Cloud Integration**: S3, Azure Blob Storage, Google Cloud Storage
7. **Custom AI Prompts Library**: Community-shared analysis templates
8. **Session Replay**: Record and replay user analysis workflows
9. **Multi-File Analysis**: Correlate events across more than two log files simultaneously
10. **Real-time AI Monitoring**: Continuous analysis with alerts

## References

- [Qt Documentation](https://doc.qt.io/)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [Design Patterns](https://refactoring.guru/design-patterns)
- [Modern C++ Design](https://www.amazon.com/Modern-Design-Generic-Programming-Patterns/dp/0201704315)
- [Plugin System Documentation](PLUGIN_SYSTEM.md)
- [AI Provider Plugin](AI_PROVIDER_PLUGIN.md)
- [Ollama Documentation](https://ollama.ai/docs)
- [OpenAI API Reference](https://platform.openai.com/docs)
- [Anthropic Claude API](https://docs.anthropic.com/)
