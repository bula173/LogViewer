# Plugin System Implementation Summary

## Overview

LogViewer implements a plugin system using **C-ABI exports** for maximum compatibility and ABI stability. Plugins are dynamically loaded shared libraries that export standard C functions.

## Current Architecture (C-ABI)

### Plugin C-ABI Exports

Plugins export standard C functions for lifecycle management and UI creation:

**Lifecycle:**
- `Plugin_Create()`: Create plugin instance, returns opaque handle
- `Plugin_Destroy(handle)`: Destroy plugin instance
- `Plugin_GetMetadataJson(handle)`: Return JSON metadata string
- `Plugin_Initialize(handle)`: Initialize plugin resources
- `Plugin_Shutdown(handle)`: Cleanup plugin resources

**Logger Integration:**
- `Plugin_SetLoggerCallback(handle, logFn)`: Application provides logging callback
  - Called immediately after `Plugin_Create()`
  - Enables plugin to log to application logs
  - Plugin uses `PluginLogger_Log(level, message)` to log

**UI Panels (Optional):**
- `Plugin_CreateMainPanel(handle, parent, settings)`: Create main content panel
- `Plugin_CreateBottomPanel(handle, parent, settings)`: Create bottom panel (e.g., chat)
- `Plugin_CreateLeftPanel(handle, parent, settings)`: Create configuration panel
- `Plugin_CreateRightPanel(handle, parent, settings)`: Create right panel

### Plugin Logger

**Problem Solved:**
Original inline header-only logger with static variables caused ODR violations across DLL boundaries - each DLL had its own copy of the global variable, causing the application's registration to be invisible to the plugin.

**Solution:**
Callback-based logging passed from application to plugin:

```cpp
// Application side (PluginManager.cpp)
auto handle = Plugin_Create();          // Create plugin instance
Plugin_SetLoggerCallback(handle, &AppPluginLog);  // Pass callback to plugin

// Plugin side (AIProviderPlugin.cpp)
void Plugin_SetLoggerCallback(PluginHandle h, PluginLogFn logFn) {
    PluginLogger_Register(logFn);  // Store in plugin's copy of global
}

// Usage anywhere in plugin
PluginLogger_Log(PLUGIN_LOG_INFO, "Plugin initialized");
```

**Benefits:**
- Works across DLL boundaries
- No ABI dependencies
- Simple usage for plugin developers
- Messages appear in application logs with `[plugin]` prefix

### Configuration UI

**Left Panel Integration:**
Plugins provide configuration UI by exporting `Plugin_CreateLeftPanel`:

```cpp
EXPORT_PLUGIN_SYMBOL void* Plugin_CreateLeftPanel(PluginHandle h, void* parent, const char* settings) {
    auto* plugin = reinterpret_cast<MyPlugin*>(h);
    QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
    return plugin->GetConfigurationUI(parentWidget);
}
```

**MainWindow Integration:**
- Checks for `Plugin_CreateLeftPanel` export
- Creates widget with `m_pluginLeftTabs` as parent
- Adds as new tab alongside "Filters" tab in left dock
- Widget lifecycle managed by host

## Plugin Manager Implementation

**Location**: `src/application/plugins/PluginManager.cpp`

**Key Features:**
- Singleton pattern for global access
- Plugin discovery in specified directory (scans for `.dll`/`.so`/`.dylib` files)
- Dynamic library loading (`LoadLibrary`/`dlopen`)
- Logger callback setup immediately after plugin creation
- Panel creation via C-ABI exports
- Platform-specific implementations (Windows, Unix)

**Loading Process:**
1. Load shared library via `LoadLibrary`/`dlopen`
2. Resolve `Plugin_Create` symbol
3. Call `Plugin_Create()` to get plugin handle
4. Call `Plugin_SetLoggerCallback` to enable logging
5. Call `Plugin_GetMetadataJson` to get plugin info
6. Call `Plugin_Initialize` to init resources
7. Create UI panels as needed via `Plugin_CreateXxxPanel`
8. On shutdown: `Plugin_Shutdown`, `Plugin_Destroy`

**Platform Support:**
- Windows: `LoadLibrary`/`GetProcAddress`/`FreeLibrary`
- Linux/macOS: `dlopen`/`dlsym`/`dlclose`

## AI Provider Plugin Example

**Location**: `plugins/ai/AIProviderPlugin.cpp`

The AI Provider plugin demonstrates the complete C-ABI architecture:

**Exports:**
- `Plugin_Create` - Creates AIProviderPlugin instance
- `Plugin_Destroy` - Destroys plugin instance
- `Plugin_SetLoggerCallback` - Receives logging callback
- `Plugin_GetMetadataJson` - Returns metadata as JSON
- `Plugin_Initialize` - Initializes resources
- `Plugin_Shutdown` - Cleanup
- `Plugin_CreateMainPanel` - AI analysis panel (main content)
- `Plugin_CreateBottomPanel` - AI chat panel (bottom dock)
- `Plugin_CreateLeftPanel` - Configuration panel (left dock)
- `Plugin_CreateAIService` - AI service factory
- `Plugin_SetAIEventsContainer` - Receives log events

**Internal Structure:**
- C++ class `AIProviderPlugin` with all logic
- C-ABI exports are thin wrappers calling C++ methods
- Uses `PluginLogger_Log` for all logging
- Proper error handling with try-catch

## Creating New Plugins

See [PLUGIN_SYSTEM.md](PLUGIN_SYSTEM.md) for detailed guide on creating plugins.

**Quick Steps:**
1. Create C++ plugin class
2. Export C-ABI functions (`Plugin_Create`, etc.)
3. Implement `Plugin_SetLoggerCallback` to enable logging
4. Optionally export panel creation functions
5. Build as shared library
6. Test loading in LogViewer

## Build System

**Plugin CMake Configuration:**

```cmake
add_library(my_plugin SHARED
    MyPlugin.cpp
)

target_include_directories(my_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src/plugin_api
)

target_link_libraries(my_plugin PRIVATE
    Qt6::Widgets
    fmt::fmt
    nlohmann_json::nlohmann_json
)

set_target_properties(my_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "my_plugin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
```

**Key Points:**
- Build as `SHARED` library
- No `lib` prefix (PREFIX "")
- Output to `${CMAKE_BINARY_DIR}/plugins`
- Link only required dependencies
- Include `src/plugin_api` for C-ABI headers

## Directory Structure

```
LogViewer/
├── src/
│   ├── plugin_api/
│   │   ├── PluginC.h              # C-ABI typedefs
│   │   ├── PluginLoggerC.h        # Logger API
│   │   ├── PluginEventsC.h        # Events API
│   │   └── PluginKeyEncryptionC.h # Encryption API
│   └── application/
│       └── plugins/
│           ├── PluginManager.hpp
│           └── PluginManager.cpp
└── plugins/
    └── ai/                         # AI provider plugin
        ├── CMakeLists.txt
        ├── AIProviderPlugin.hpp
        ├── AIProviderPlugin.cpp
        └── lib/                    # Dependencies
```

## Usage Examples

### Plugin Manager

```cpp
#include "application/plugins/PluginManager.hpp"

auto& manager = plugin::PluginManager::GetInstance();
manager.SetPluginsDirectory("plugins/");

// Load plugin
auto result = manager.LoadPlugin("plugins/ai_provider.dll");
if (result.isOk()) {
    std::string id = result.unwrap();
    std::cout << "Loaded plugin: " << id << std::endl;
}
```

### Creating UI Panels

```cpp
// Application code (MainWindow.cpp)
void* parentWidget = m_pluginLeftTabs;  // QTabWidget*
void* panel = Plugin_CreateLeftPanel(handle, parentWidget, nullptr);
if (panel) {
    QWidget* widget = reinterpret_cast<QWidget*>(panel);
    m_pluginLeftTabs->addTab(widget, "AI Provider");
}
```

## Key Design Decisions

1. **C-ABI Only**: No C++ interfaces exported - maximum compatibility
2. **Callback-based Logging**: Solves ODR violation across DLL boundaries
3. **Opaque Handles**: `PluginHandle` is `void*` - hides implementation
4. **Qt Widget Panels**: UI created as `QWidget*` cast to `void*`
5. **JSON Metadata**: Metadata returned as JSON string - flexible schema
6. **Manual Memory**: Plugin allocates, app may free or plugin manages

## Known Limitations

1. **No Hot Reloading**: Must restart application to reload plugins
2. **No Sandboxing**: Plugins run in same process - no security isolation
3. **ABI Compatibility**: Must match compiler and Qt version
4. **Symbol Visibility**: Must use `EXPORT_PLUGIN_SYMBOL` for all exports

## Testing

**Manual Testing:**
1. Build plugin as shared library
2. Copy to plugins directory
3. Start LogViewer
4. Check application logs for plugin loading messages
5. Verify UI panels appear correctly

**Troubleshooting:**
- Check plugin exports: `nm -g plugin.so | grep Plugin_`
- Review logs for error messages
- Verify plugin dependencies are satisfied
- Ensure ABI compatibility (same compiler/Qt)

## Success Criteria

✅ **Completed**:
- C-ABI plugin architecture
- Logger callback mechanism
- Panel creation exports
- AI Provider plugin with 3 panels
- PluginManager implementation
- Cross-platform support (Windows, Linux, macOS)
- Comprehensive documentation

## Future Enhancements

- Plugin API versioning
- Hot reload support
- Plugin marketplace
- Automatic updates
- Sandboxing/isolation
- Performance monitoring

## Conclusion

The C-ABI plugin system provides a stable, maintainable foundation for extending LogViewer. The architecture has been successfully implemented and tested with the AI Provider plugin, demonstrating:

- **Stability**: Callback-based logging solves ODR violations
- **Flexibility**: Optional panel exports support various UI needs
- **Maintainability**: Clean separation between app and plugins
- **Compatibility**: Pure C-ABI works across compilers and versions

The system is production-ready and new plugins can be added following the established patterns.

