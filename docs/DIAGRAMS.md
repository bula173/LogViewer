# LogViewer Architecture and Workflow Diagrams

This document provides an index and explanation of all PlantUML diagrams in the LogViewer documentation. These diagrams visualize the system architecture, component relationships, and key workflows.

## Table of Contents

1. [System Architecture Overview](#system-architecture-overview)
2. [Component Diagrams](#component-diagrams)
3. [Sequence Diagrams](#sequence-diagrams)

---

## System Architecture Overview

### architecture-overview.puml

**Purpose**: High-level system architecture showing all major components and external integrations.

**Components**:
- **Qt UI**: User interface layer
- **Main Controller**: Business logic orchestration
- **Data Models**: Core entities (LogEvent, EventsContainer)
- **Parser System**: Handles XML, CSV, and custom log format parsing
- **Filter System**: Applies and manages filters
- **Configuration**: Manages settings and persistence
- **AI Provider**: Plugin-based AI analysis (Ollama, OpenAI, Claude, Gemini)
- **Plugin System**: Dynamic plugin loading and management

**External Systems**:
- Log files (input data)
- Ollama (local AI inference)
- OpenAI / Cloud AI services

**Key Relationships**:
- User interacts with UI
- UI consumes Controller logic
- Controller manages Models, Parsers, and Filters
- Parser populates Models with data
- Filters operate on Models
- AI Provider connects to external services
- Plugin Manager loads and manages plugins

---

## Component Diagrams

### data-model.puml

**Purpose**: Details the core data model classes and their relationships.

**Key Classes**:
- **LogEvent**: Represents a single log entry with key-value pairs
  - Fast O(1) key lookup via internal hash index
  - Supports original ID tracking and source attribution
  - Memory-efficient through move semantics

- **EventsContainer**: Thread-safe collection of LogEvents
  - Uses std::shared_mutex for reader-writer locking
  - Supports batch operations for performance
  - Implements IModel interface for MVC
  - Notifies observers when data changes

- **IModel**: Interface defining container behavior
  - Observer pattern for view notifications
  - Abstract methods for item management

**Thread Safety**: EventsContainer provides thread-safe access through shared_mutex (readers can run concurrently, writers get exclusive access)

---

### filter-system.puml

**Purpose**: Details the filter system architecture using the Strategy pattern.

**Key Classes**:
- **FilterCondition**: Single condition within a filter
  - Can filter on column values or nested parameters
  - Supports case-sensitive/insensitive matching
  - Uses pluggable IFilterStrategy for evaluation

- **Filter**: Collection of conditions with AND logic
  - Can be inverted for negation
  - Supports complex nested parameter searches
  - Applies to log events or indices

- **FilterManager**: Singleton managing all active filters
  - Persists filters to JSON configuration
  - Applies multiple filters with optimized set operations
  - Notifies views when filtering results change

- **IFilterStrategy**: Abstract interface for matching algorithms
  - **RegexFilterStrategy**: Pattern matching with regex
  - **TextFilterStrategy**: Simple text substring matching
  - Extensible for custom strategies

**Design Pattern**: Strategy pattern allows swappable matching algorithms

---

### plugin-system.puml

**Purpose**: Details the plugin system architecture enabling extensibility.

**Key Interfaces**:
- **IPlugin**: Base interface all plugins implement (C-ABI compatible)
  - GetMetadata() returns plugin information
  - Initialize/Shutdown lifecycle methods
  - Cross-compiler compatible

- **IAIProvider**: Plugins that provide AI analysis
  - AnalyzeEvents() performs analysis
  - GetCapabilities() describes features

- **IParser**: Plugins that parse log formats
  - ParseFile() converts files to LogEvents
  - SupportsFormat() checks file type

- **IUIPanel**: Plugins that provide UI panels
  - GetWidget() returns Qt widget
  - Event callbacks for updates

**PluginManager**:
- Singleton that loads, manages, and discovers plugins
- Uses PluginLoader to dynamically load from DLLs/SOs/ZIPs
- Notifies observers (PluginObserver) of plugin lifecycle events
- C-ABI ensures cross-compiler compatibility

**Design Patterns**:
- Factory pattern (PluginLoader creates plugin instances)
- Registry pattern (PluginManager maintains plugin registry)
- Observer pattern (PluginObserver notifications)

---

## Sequence Diagrams

### log-loading-workflow.puml

**Purpose**: Shows the complete workflow from user selecting a log file through display.

**Workflow**:
1. User loads a log file through MainWindow
2. MainWindow delegates to Parser
3. Parser reads file (XML/CSV) and creates LogEvents
4. Parser calls EventsContainer.AddEventBatch() (thread-safe)
5. EventsContainer acquires write lock, stores events
6. EventsContainer notifies UIModel of changes
7. UIModel updates EventsTableView model
8. EventsTableView performs virtual scrolling (only renders visible rows)
9. Events display to user

**Performance**: Virtual scrolling enables handling of millions of events

---

### filter-workflow.puml

**Purpose**: Shows how filters are created, applied, and updated.

**Workflow**:
1. User creates filter in FilterPanel
2. FilterPanel adds filter to FilterManager
3. FilterManager compiles filter (builds regex, initializes strategy)
4. User applies filter
5. FilterManager evaluates each filter against events in EventsContainer
6. For each event:
   - Filter.matches() evaluates all conditions (AND logic)
   - Matching indices are accumulated
7. UIModel is notified with filtered results
8. EventsTableView updates to show only matching events
9. User can disable/remove filters to show all events again

**Design**: Multiple filters are applied sequentially, maintaining set intersection semantics

---

### ai-analysis-workflow.puml

**Purpose**: Shows AI analysis request flow from user action to results display.

**Workflow**:
1. User selects events in AIAnalysisPanel
2. User chooses AI provider (Ollama, OpenAI, Claude, Gemini, etc.)
3. AIAnalysisPanel requests provider from PluginManager
4. PluginManager returns IAIProvider instance
5. AIProvider.AnalyzeEvents() is called
6. Provider sends HTTP request to external AI service
7. External service processes request:
   - Ollama: Local inference (no internet required)
   - OpenAI/Claude/Gemini: Cloud APIs
8. Response is returned to provider
9. Provider processes/formats response
10. Results are displayed in ResultsDisplay
11. User can export/copy analysis

**Extensibility**: New AI providers can be added as plugins

---

### plugin-loading-workflow.puml

**Purpose**: Shows complete plugin lifecycle from discovery through loading and cleanup.

**Lifecycle**:

**Loading Phase**:
1. Application calls PluginManager.DiscoverPlugins()
2. PluginManager scans directories for .so/.dll/.zip files
3. For each file:
   - PluginLoader validates file (signature check)
   - Opens dynamic library with dlopen()
   - Extracts metadata from library
   - Calls plugin_create() (C-ABI function)
   - Plugin creates IPlugin instance
   - PluginLoader returns PluginHandle
4. PluginManager initializes plugin
5. Plugin registers event callbacks, setup resources
6. Plugin is registered in PluginManager's registry

**Unloading Phase**:
1. Application calls UnloadPlugin()
2. PluginManager calls plugin.Shutdown()
3. Plugin cleans up resources
4. Dynamic library is closed

**C-ABI Benefits**:
- Plugins can be compiled with any C-compatible compiler
- Cross-platform (Windows, Linux, macOS)
- Cross-compiler (MSVC, GCC, Clang)
- Stable binary interface across versions

---

## Using These Diagrams

### Generating Images

**Option 1: VS Code Extension**
- Install "Markdown Preview Mermaid Support" or "PlantUML" extension
- Open .puml files in VS Code preview

**Option 2: PlantUML Online**
- Visit https://www.plantuml.com/plantuml/uml/
- Copy and paste diagram content

**Option 3: Command Line**
```bash
# Install plantuml (macOS)
brew install plantuml

# Generate PNG/SVG
plantuml docs/diagrams/*.puml
plantuml -tsvg docs/diagrams/*.puml
```

### Integration with Doxygen

Reference diagrams from Doxygen documentation:
```cpp
/**
 * @class MyClass
 * @brief Description
 *
 * @image html ../diagrams/related-diagram.png
 * @image latex ../diagrams/related-diagram.eps
 */
```

---

## Maintenance Notes

**When to Update Diagrams**:
- After significant architectural changes
- When new major components are added
- When core relationships change

**How to Update**:
1. Modify the .puml file in `docs/diagrams/`
2. Regenerate images using PlantUML
3. Update diagram descriptions above if scope changed
4. Commit both .puml and generated images

---

## Related Documentation

- See `src/application/db/LogEvent.hpp` for data model details
- See `src/application/plugins/IPlugin.hpp` for plugin interface
- See  `ARCHITECTURE.md` for comprehensive architecture documentation
- See `PLUGIN_SYSTEM.md` for plugin development guide

