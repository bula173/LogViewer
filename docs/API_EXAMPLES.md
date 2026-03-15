# LogViewer API Usage Examples

This guide provides practical examples for using the key LogViewer APIs. Perfect for developers integrating LogViewer components into their applications or extending the system.

## Table of Contents

1. [Working with LogEvents](#working-with-logevents)
2. [Managing Large Log Collections](#managing-large-log-collections)
3. [Creating and Applying Filters](#creating-and-applying-filters)
4. [Parsing Different Log Formats](#parsing-different-log-formats)
5. [Loading and Using Plugins](#loading-and-using-plugins)
6. [AI-Assisted Log Analysis](#ai-assisted-log-analysis)
7. [Configuration Management](#configuration-management)

---

## Working with LogEvents

### Creating Basic LogEvents

```cpp
#include "LogEvent.hpp"

// Create from initializer list
db::LogEvent event1(42, {
    {"timestamp", "2025-03-15T12:00:00Z"},
    {"level", "ERROR"},
    {"message", "Database connection failed"},
    {"source", "database_engine"}
});

// Create from vector of pairs
std::vector<std::pair<std::string, std::string>> data = {
    {"timestamp", "2025-03-15T12:01:00Z"},
    {"level", "INFO"},
    {"message", "Connection recovered"}
};
db::LogEvent event2(43, data);
db::LogEvent event3(44, std::move(data));  // Using move for efficiency
```

### Accessing Event Data

```cpp
// Look up single value (O(1) via internal hash index)
std::string timestamp = event1.findByKey("timestamp");
if (timestamp.empty()) {
    std::cout << "Timestamp not found\n";
}

// Iterate through event items (structured binding - C++20)
for (const auto& [key, value] : event1.getEventItems()) {
    std::cout << key << " = " << value << "\n";
}

// Get event ID
int id = event1.getId();

// Track source and original ID
event1.SetSource("log_file_1.txt");
event1.SetOriginalId(42);  // Original ID before merge
event1.SetId(1000);         // New merged ID
```

### Converting Events to/from JSON

```cpp
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Convert event to JSON
json j = event1.toJson();
std::cout << j.dump(2) << "\n";

// Create event from JSON
json j_input = json::parse(R"({
    "id": 50,
    "items": [
        ["timestamp", "2025-03-15T12:30:00Z"],
        ["level", "WARN"]
    ]
})");
db::LogEvent event_from_json = db::LogEvent::fromJson(j_input);
```

---

## Managing Large Log Collections

### Using EventsContainer for Thread-Safe Access

```cpp
#include "EventsContainer.hpp"
#include <thread>

// Create container
auto container = std::make_shared<db::EventsContainer>();

// Add single events (thread-safe)
db::LogEvent event(1, {{"msg", "test"}});
container->AddEvent(std::move(event));

// Add batch of events efficiently
std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
for (int i = 2; i <= 100; ++i) {
    batch.push_back({i, {{"msg", "event " + std::to_string(i)}}});
}
container->AddEventBatch(std::move(batch));

// Reader thread
std::thread reader([container]() {
    // Shared lock allows concurrent reads
    for (int i = 0; i < container->Size(); ++i) {
        const auto& event = container->GetEvent(i);
        std::cout << "Event: " << event.getId() << "\n";
    }
});

// Query from container
std::string msg = container->FindByKey(0, "msg");
int size = container->Size();

reader.join();
```

### Handling Model Changes (Observer Pattern)

```cpp
// Register for change notifications
std::function<void()> on_data_updated = []{
    std::cout << "Data updated, refresh UI\n";
};
container->RegisterOnDataUpdated(on_data_updated);

// When data changes, observer is called
container->Clear();  // Internally calls NotifyDataChanged()

// Unregister when done
container->UnregisterOnDataUpdated(on_data_updated);
```

---

## Creating and Applying Filters

### Building Simple Filters

```cpp
#include "Filter.hpp"
#include "FilterManager.hpp"

// Create filter with regex matching
filters::Filter error_filter(
    "ErrorOnly",        // name
    "level",           // column to search
    "^(ERROR|FATAL)$", // regex pattern
    true,              // case sensitive
    false,             // not inverted
    false,             // not a parameter filter
    "",                // no parameter key
    0                  // depth
);

error_filter.compile();  // Build internal regex

// Add to global filter manager
auto& fm = filters::FilterManager::GetInstance();
fm.addFilter(std::make_unique<filters::Filter>(error_filter));
```

### Complex Filters with Multiple Conditions

```cpp
// Create filter with multiple conditions (AND logic)
filters::Filter complex_filter("ErrorWithSource");
complex_filter.addCondition(filters::FilterCondition(
    "level", "ERROR", false, "", 0, true
));
complex_filter.addCondition(filters::FilterCondition(
    "source", "database", false, "", 0, false
));
complex_filter.compile();
```

### Applying Filters to Data

```cpp
// Apply active filters
std::vector<unsigned long> matching_indices = fm.applyFilters(0, 100, *container);

std::cout << "Matching events: " << matching_indices.size() << "\n";

// Display matching events
for (auto idx : matching_indices) {
    const auto& event = container->GetEvent(idx);
    // Display...
}

// Disable/enable filters
fm.enableFilter("ErrorOnly", false);

// Get fresh results
auto new_results = fm.applyFilters(0, 100, *container);
```

### Persisting Filters

```cpp
// Save filters to JSON configuration
fm.SaveFiltersToJson("filters.json");

// Load filters from file
fm.LoadFiltersFromJson("filters.json");
```

---

## Parsing Different Log Formats

### Using ParserFactory for Format Detection

```cpp
#include "ParserFactory.hpp"

auto parser_result = parser::ParserFactory::GetParser("application.xml");
if (parser_result) {
    auto parser = std::move(parser_result.unwrap());

    // Parse file
    auto parse_result = parser->ParseData("application.xml");
    if (parse_result) {
        std::cout << "Parsed successfully\n";
        auto uint_progress = parse_result.unwrap();
    } else {
        std::cerr << "Parse error: " << parse_result.error().Message() << "\n";
    }
} else {
    std::cerr << "No parser for this format\n";
}
```

### Custom Parser Registration

```cpp
// Implement custom parser
class CustomLogParser : public parser::IDataParser {
    void ParseData(const std::filesystem::path& filepath) override {
        // Custom parsing logic
    }

    uint32_t GetCurrentProgress() const override { return 0; }
    uint32_t GetTotalProgress() const override { return 100; }
    // ... other required methods
};

// Register with factory
parser::ParserFactory::RegisterParser("custom", std::make_unique<CustomLogParser>());
```

---

## Loading and Using Plugins

### Basic Plugin Loading

```cpp
#include "PluginManager.hpp"

auto& pm = plugin::PluginManager::GetInstance();

// Discover plugins in directory
pm.DiscoverPlugins("./plugins");

// Load specific plugin
auto load_result = pm.LoadPlugin("./plugins/ai_provider.so");
if (load_result) {
    std::cout << "Plugin loaded successfully\n";
} else {
    std::cerr << "Load failed: " << load_result.error().Message() << "\n";
}

// Get loaded plugin
auto ai_plugin = pm.GetPlugin("ai_provider");
if (ai_plugin) {
    auto metadata = ai_plugin->GetMetadata();
    std::cout << "Plugin: " << metadata.name << " v" << metadata.version << "\n";
}
```

### Working with Specific Plugin Types

```cpp
// Get all plugins of specific type
auto ai_plugins = pm.GetPluginsOfType(plugin::PluginType::AI_PROVIDER);
for (auto* plugin : ai_plugins) {
    auto metadata = plugin->GetMetadata();
    std::cout << "AI Provider: " << metadata.name << "\n";
}

// Use AI provider plugin
if (auto* ai = pm.GetPlugin("ollama_provider")) {
    auto* ai_provider = dynamic_cast<plugin::IAIProvider*>(ai);
    if (ai_provider) {
        std::vector<db::LogEvent> events_to_analyze = {/*...*/};
        auto analysis = ai_provider->AnalyzeEvents(events_to_analyze);
        // Process results
    }
}
```

### Observing Plugin Lifecycle

```cpp
class MyPluginObserver : public plugin::PluginObserver {
    void OnPluginLoaded(plugin::IPlugin* plugin) override {
        std::cout << "Plugin loaded: " << plugin->GetName() << "\n";
    }

    void OnPluginUnloaded(plugin::IPlugin* plugin) override {
        std::cout << "Plugin unloaded: " << plugin->GetName() << "\n";
    }

    void OnPluginError(const std::string& id, const std::string& error) override {
        std::cerr << "Plugin error in " << id << ": " << error << "\n";
    }
};

auto observer = std::make_unique<MyPluginObserver>();
pm.RegisterPluginObserver(observer.get());

// Plugin events will now be logged
pm.LoadPlugin("./plugins/test.so");
```

---

## AI-Assisted Log Analysis

### Analyzing Events with AI Provider

```cpp
// Select events to analyze
std::vector<db::LogEvent> selected_events = {/*...selected from UI...*/};

// Get AI provider plugin
auto& pm = plugin::PluginManager::GetInstance();
auto* ai_plugin = pm.GetPlugin("openai_provider");
auto* ai = dynamic_cast<plugin::IAIProvider*>(ai_plugin);

if (ai) {
    // Perform analysis
    auto analysis_result = ai->AnalyzeEvents(selected_events);

    if (analysis_result) {
        std::string insights = analysis_result.unwrap();
        std::cout << "AI Analysis Results:\n" << insights << "\n";
    } else {
        std::cerr << "Analysis error: " << analysis_result.error().Message() << "\n";
    }
}
```

### Checking AI Provider Capabilities

```cpp
if (auto* ai = dynamic_cast<plugin::IAIProvider*>(ai_plugin)) {
    auto capabilities = ai->GetCapabilities();

    std::cout << "AI Provider supports:\n";
    for (const auto& cap : capabilities) {
        std::cout << "  - " << cap << "\n";
    }

    // Only use supported capabilities
    if (std::find(capabilities.begin(), capabilities.end(), "pattern_detection")
        != capabilities.end()) {
        // Use pattern detection
    }
}
```

---

## Configuration Management

### Loading and Saving Configuration

```cpp
#include "Config.hpp"

auto& config = config::Config::GetInstance();

// Configuration is auto-loaded from platform-specific location:
// macOS: ~/Library/Application Support/LogViewer/config.json
// Linux: ~/.config/LogViewer/config.json
// Windows: %APPDATA%/LogViewer/config.json

// Access configuration
auto app_name = config.GetAppName();
auto version = config.GetConfigVersion();

// Modify and save
config.SetColumnWidth("timestamp", 150);
config.SaveConfig();

// Reload from file
config.Reload();
```

### Working with Named Colors

```cpp
// Set column colors
std::map<std::string, std::string> color_map = {
    {"ERROR", "#FF0000"},
    {"WARN", "#FFFF00"},
    {"INFO", "#00FF00"}
};
config.SetColumnColors("level", color_map);

// Retrieve colors
auto error_color = config.GetColumnColor("level", "ERROR");
std::cout << "Error color: " << error_color << "\n";
```

### Persistent Filter Configuration

```cpp
// Filters auto-save when modified
auto& fm = filters::FilterManager::GetInstance();
fm.addFilter(std::make_unique<filters::Filter>(/* ... */));

// Filters are automatically persisted in config.json
// On next startup, they are automatically restored
```

---

## Error Handling with Result Types

```cpp
#include "Result.hpp"

// Using Result type for error handling
util::Result<int, error::Error> parse_number(const std::string& str) {
    try {
        return util::Result<int, error::Error>::Ok(std::stoi(str));
    } catch (const std::exception& e) {
        return util::Result<int, error::Error>::Err(
            error::Error(1, std::string("Parse error: ") + e.what())
        );
    }
}

// Using the result
auto result = parse_number("42");
if (result) {
    int value = result.unwrap();
    std::cout << "Parsed value: " << value << "\n";
} else {
    std::cerr << "Error: " << result.error().Message() << "\n";
}

// Chain operations with andThen
auto chain_result = parse_number("100")
    .andThen([](int x) {
        if (x > 0) {
            return util::Result<int, error::Error>::Ok(x * 2);
        } else {
            return util::Result<int, error::Error>::Err(
                error::Error(1, "Value must be positive")
            );
        }
    })
    .map([](int x) { return x + 10; });
```

---

## Best Practices

### Memory Management
- Use `std::move()` for efficiency when passing large collections
- Prefer `std::unique_ptr` for owned resources
- Use `std::shared_ptr` for shared ownership
- EventsContainer is thread-safe; use it for concurrent access

### Thread Safety
- Don't access LogEvent from multiple threads without external synchronization
- Use EventsContainer for thread-safe multi-threaded access
- FilterManager uses locks internally; safe for concurrent access

### Performance
- Use structured bindings (C++20) for iteration: `for (const auto& [key, val] : items)`
- Batch operations when possible: `AddEventBatch()` is more efficient than `AddEvent()` in loops
- Virtual scrolling in UI handles millions of events efficiently

### Error Handling
- Use `Result<T, E>` types for fallible operations
- Chain operations with `map()`, `andThen()`, `or_else()`
- Check error messages for debugging

---

## For More Information

- See `docs/ARCHITECTURE.md` for system design
- See `docs/PLUGIN_SYSTEM.md` for plugin development
- See `src/application/util/Result.hpp` for Result API documentation
- See `Doxyfile` generated HTML documentation in `docs/html/`
