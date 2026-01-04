# LogViewer Plugin System

## Quick Start

New to LogViewer plugins? Start here:
- **[SDK Getting Started Guide](SDK_GETTING_STARTED.md)** — Complete walkthrough for plugin developers
- **[SDK Quick Reference](SDK_QUICK_REFERENCE.md)** — Quick syntax reference and examples
- **[Basic Plugin Example](../examples/BasicPlugin/)** — Minimal working plugin to learn from

## Overview

LogViewer supports a plugin architecture that allows extending functionality without modifying the core application. Plugins are dynamically loaded shared libraries that use a **C-ABI interface** for maximum compatibility and ABI stability across different compilers and versions.

Plugins export standard C functions like `Plugin_Create`, `Plugin_CreateMainPanel`, etc.

### C-ABI Plugin Architecture

All modern LogViewer plugins use the **C-ABI approach** for better compatibility and reduced ABI dependencies. Plugins export standard C functions that the application calls to create and manage plugin instances. This is the only supported method for plugin development going forward.

**Required Exports:**
```cpp
extern "C" {
    EXPORT_PLUGIN_SYMBOL PluginHandle Plugin_Create();
    EXPORT_PLUGIN_SYMBOL void Plugin_Destroy(PluginHandle);
    EXPORT_PLUGIN_SYMBOL const char* Plugin_GetMetadataJson(PluginHandle);
}
```

**Optional Panel Exports:**
```cpp
extern "C" {
    EXPORT_PLUGIN_SYMBOL void* Plugin_CreateMainPanel(PluginHandle, void* parent, const char* settings);
    EXPORT_PLUGIN_SYMBOL void* Plugin_CreateBottomPanel(PluginHandle, void* parent, const char* settings);
    EXPORT_PLUGIN_SYMBOL void* Plugin_CreateLeftPanel(PluginHandle, void* parent, const char* settings);
    EXPORT_PLUGIN_SYMBOL void* Plugin_CreateRightPanel(PluginHandle, void* parent, const char* settings);
}
```

**Logger Callback:**
```cpp
extern "C" {
    EXPORT_PLUGIN_SYMBOL void Plugin_SetLoggerCallback(PluginHandle, PluginLogFn);
}
```

The application calls `Plugin_SetLoggerCallback` immediately after `Plugin_Create` to provide a logging function that plugins can use.

## Plugin Logging

Plugins can log messages to the application's logging system using the provided callback:

```cpp
#include "PluginLoggerC.h"

// Use the logger anywhere in plugin code
PluginLogger_Log(PLUGIN_LOG_INFO, "Plugin initialized successfully");
PluginLogger_Log(PLUGIN_LOG_ERROR, "Failed to connect to service");
```

**Log Levels:**
- `PLUGIN_LOG_TRACE`: Detailed debug information
- `PLUGIN_LOG_DEBUG`: Debug messages
- `PLUGIN_LOG_INFO`: Informational messages
- `PLUGIN_LOG_WARN`: Warning messages
- `PLUGIN_LOG_ERROR`: Error messages
- `PLUGIN_LOG_CRITICAL`: Critical errors

**How It Works:**
1. Application calls `Plugin_Create()` to instantiate the plugin
2. Application calls `Plugin_SetLoggerCallback(handle, &AppLogFunc)` to provide logging callback
3. Plugin calls `PluginLogger_Register(logFn)` to store the callback
4. Plugin uses `PluginLogger_Log(level, message)` to log messages
5. Messages appear in application logs with `[plugin]` prefix

## Plugin Types

The system supports several plugin types:

- **Parser**: Custom log file format parsers
- **Filter**: Custom filtering strategies
- **Exporter**: Export logs to different formats
- **Analyzer**: Advanced log analysis tools
- **Connector**: Remote log source connections (e.g., SSH, databases)
- **Visualizer**: Custom visualization components
- **AIProvider**: AI-powered log analysis
- **Custom**: Generic plugins for other purposes

## Plugin Architecture

### C Plugin Structure

Plugins are C++ classes that export C-ABI functions. Internally, plugins can use any C++ features, but the external interface must be pure C.

**Example Plugin Class:**

```cpp
namespace plugin {

class MyPlugin {
public:
    MyPlugin() { /* constructor */ }
    ~MyPlugin() { /* destructor */ }
    
    PluginMetadata GetMetadata() const {
        return {
            .id = "my_plugin",
            .name = "My Plugin",
            .version = "1.0.0",
            .type = PluginType::Custom
        };
    }
    
    bool Initialize() { /* init resources */ return true; }
    void Shutdown() { /* cleanup */ }
    
    // Optional: Create UI panels
    QWidget* CreateConfigPanel(QWidget* parent) { /* ... */ }
};

} // namespace plugin
```

### Plugin Metadata

Plugins return metadata as JSON via `Plugin_GetMetadataJson`:

```json
{
    "id": "my_plugin",
    "name": "My Plugin",
    "version": "1.0.0",
    "api_version": "1.0.0",
    "description": "Plugin description"
}
```

See the [SDK Quick Reference](SDK_QUICK_REFERENCE.md#configjson-template) for the current config.json format and required fields.

## Plugin Manager

The `PluginManager` singleton handles plugin lifecycle:

- **Discovery**: Scan plugins directory for `.dll`/`.so`/`.dylib` files
- **Loading**: Load plugin via `LoadLibrary`/`dlopen`
- **Logger Setup**: Call `Plugin_SetLoggerCallback` to provide logging
- **Initialization**: Call `Plugin_Initialize` on loaded plugins
- **Panel Creation**: Call `Plugin_CreateXxxPanel` functions for UI
- **Lifecycle**: Initialize, shutdown plugins via C-ABI functions

### Plugin Loading Process

```cpp
// 1. Application loads plugin library
PluginHandle handle = Plugin_Create();

// 2. Set up logging immediately
Plugin_SetLoggerCallback(handle, &AppPluginLog);

// 3. Get plugin metadata
const char* json = Plugin_GetMetadataJson(handle);
// Parse JSON to get plugin info

// 4. Initialize plugin
Plugin_Initialize(handle);

// 5. Create UI panels as needed
void* leftPanel = Plugin_CreateLeftPanel(handle, parentWidget, settingsJson);
void* mainPanel = Plugin_CreateMainPanel(handle, parentWidget, settingsJson);

// 6. Cleanup on exit
Plugin_Shutdown(handle);
Plugin_Destroy(handle);
```

## Creating a Plugin

**⚠️ For detailed plugin development instructions, see:**
- [SDK Getting Started Guide](SDK_GETTING_STARTED.md) - Complete step-by-step guide
- [SDK Quick Reference](SDK_QUICK_REFERENCE.md) - Quick syntax reference
- [Basic Plugin Example](../examples/BasicPlugin/) - Working minimal example

The sections below provide architectural context. Developers should follow the SDK guides above.

### 1. Create Plugin Class

Create a C++ class with your plugin logic:

```cpp
namespace plugin {

class MyPlugin {
private:
    std::string m_lastError;
    
public:
    MyPlugin() = default;
    ~MyPlugin() = default;
    
    PluginMetadata GetMetadata() const {
        return {
            .id = "my_plugin",
            .name = "My Plugin",
            .version = "1.0.0",
            .apiVersion = "1.0.0",
            .author = "Your Name",
            .description = "Plugin description",
            .type = PluginType::Custom
        };
    }
    
    bool Initialize() {
        PluginLogger_Log(PLUGIN_LOG_INFO, "Initializing plugin");
        return true;
    }
    
    void Shutdown() {
        PluginLogger_Log(PLUGIN_LOG_INFO, "Shutting down plugin");
    }
    
    const std::string& GetLastError() const { return m_lastError; }
};

} // namespace plugin
```

### 2. Export C-ABI Functions

Export required C functions:

```cpp
extern "C" {

EXPORT_PLUGIN_SYMBOL PluginHandle Plugin_Create() {
    try {
        return new plugin::MyPlugin();
    } catch (...) {
        return nullptr;
    }
}

EXPORT_PLUGIN_SYMBOL void Plugin_Destroy(PluginHandle h) {
    if (h) {
        delete reinterpret_cast<plugin::MyPlugin*>(h);
    }
}

EXPORT_PLUGIN_SYMBOL void Plugin_SetLoggerCallback(PluginHandle h, PluginLogFn logFn) {
    if (h && logFn) {
        PluginLogger_Register(logFn);
    }
}

EXPORT_PLUGIN_SYMBOL const char* Plugin_GetMetadataJson(PluginHandle h) {
    if (!h) return nullptr;
    auto* plugin = reinterpret_cast<plugin::MyPlugin*>(h);
    auto meta = plugin->GetMetadata();
    
    // Convert metadata to JSON and return as malloc'd string
    nlohmann::json j;
    j["id"] = meta.id;
    j["name"] = meta.name;
    j["version"] = meta.version;
    // ... add other fields
    
    std::string str = j.dump();
    char* result = (char*)malloc(str.size() + 1);
    if (result) {
        memcpy(result, str.c_str(), str.size() + 1);
    }
    return result;
}

} // extern "C"
```

### 3. Build as Shared Library

**Use the SDK approach:**
```cmake
cmake_minimum_required(VERSION 3.22)
project(MyPlugin)

find_package(LogViewer CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

add_library(my_plugin MODULE src/MyPlugin.cpp)

target_link_libraries(my_plugin
    PRIVATE
        LogViewer::plugin_api
        Qt6::Widgets
)

set_target_properties(my_plugin PROPERTIES PREFIX "")

configure_file(config.json.in config.json @ONLY)
add_custom_command(TARGET my_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/config.json
        $<TARGET_FILE_DIR:my_plugin>/config.json
)
```

See [SDK Quick Reference](SDK_QUICK_REFERENCE.md#cmakeliststxt-template) for the complete CMakeLists.txt template.

## Available Plugins

### Basic Example Plugin

**Location**: `examples/BasicPlugin/`

A minimal plugin demonstrating the C-ABI interface. Perfect for learning plugin development.

**Features**:
- All required plugin exports
- Plugin state management
- Logger callback integration
- Config.json metadata

**Build Instructions**:
```bash
cd examples/BasicPlugin
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk/lib/cmake
cmake --build build
```

### AI Provider Plugin

**Location**: `plugins/ai/`

Provides AI-powered log analysis capabilities with multiple provider support.

**Features**:
- Multiple AI provider support (Ollama, OpenAI, Anthropic, Google Gemini, xAI Grok, LM Studio)
- Three UI panels: configuration (left), analysis (main), chat (bottom)
- Real-time log analysis and pattern detection
- Interactive chat with AI about logs

**Build Option**: Enabled by default when building LogViewer with plugins

**Dependencies**: libcurl, Qt6, nlohmann_json, fmt

See [AI_PROVIDER_PLUGIN.md](AI_PROVIDER_PLUGIN.md) for detailed documentation.

## Plugin Configuration

Plugins are configured through the application's configuration dialog:

1. **Plugin List**: View all discovered plugins
2. **Enable/Disable**: Toggle plugin activation
3. **Auto-Load**: Configure plugins to load automatically on startup
4. **Settings**: Plugin-specific configuration via left panel UI

Plugin settings are stored in JSON format in the application's configuration directory.

## Plugin Directory Structure

```
LogViewer/
├── src/
│   ├── plugin_api/
│   │   ├── PluginC.h              # C-ABI function typedefs
│   │   ├── PluginLoggerC.h        # Logger API for plugins
│   │   ├── PluginEventsC.h        # Events API
│   │   └── PluginKeyEncryptionC.h # Key encryption API
│   └── application/
│       └── plugins/
│           ├── PluginManager.hpp  # Plugin manager
│           └── PluginManager.cpp
└── plugins/
    └── ai/                         # AI provider plugin
        ├── CMakeLists.txt
        ├── AIProviderPlugin.hpp
        ├── AIProviderPlugin.cpp
        └── lib/                    # Plugin dependencies
```

## Platform-Specific Notes

### macOS
- Plugin extension: `.dylib`
- Use `dlopen()` for dynamic loading
- Plugins built with: `add_library(plugin SHARED)`

### Linux
- Plugin extension: `.so`
- Use `dlopen()` for dynamic loading
- Requires `-fPIC` compiler flag

### Windows
- Plugin extension: `.dll`
- Use `LoadLibrary()` for dynamic loading
- Use `EXPORT_PLUGIN_SYMBOL __declspec(dllexport)` for exports

## Troubleshooting

### Plugin Not Loading

1. Check plugin file exists in plugins directory
2. Verify plugin exports `Plugin_Create` function: `nm -g plugin.so | grep Plugin_Create`
3. Check dependencies are satisfied
4. Review application logs for error messages
5. Ensure plugin is built with compatible compiler/ABI

### Logger Issues

1. Verify `Plugin_SetLoggerCallback` is exported
2. Check that `PluginLogger_Register` is called in the callback
3. Use `PLUGIN_LOG_INFO` level for visibility (DEBUG may be filtered)
4. Look for `[plugin]` prefix in application logs

### Panel Not Appearing

1. Check that `Plugin_CreateXxxPanel` export exists
2. Verify function returns non-null `QWidget*`
3. Check plugin logs for error messages during panel creation
4. Ensure parent widget is passed correctly

### Best Practices

1. **Error Handling**: Always check return values, use try-catch in C++ code
2. **Logging**: Use `PluginLogger_Log` extensively for debugging
3. **Dependencies**: Keep external dependencies minimal
4. **Versioning**: Use semantic versioning for plugins
5. **Documentation**: Provide clear plugin documentation
6. **Testing**: Test plugin loading/unloading multiple times
7. **Thread Safety**: Ensure plugins are thread-safe
8. **Resource Cleanup**: Properly cleanup in `Plugin_Destroy`

## Future Enhancements

- Plugin hot-reloading support
- Plugin marketplace/repository
- Automatic dependency resolution
- Plugin sandboxing for security
- Plugin API versioning
- Remote plugin updates

