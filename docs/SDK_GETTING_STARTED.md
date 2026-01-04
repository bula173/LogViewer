# LogViewer Plugin SDK - Getting Started Guide

This guide walks plugin developers through building plugins for LogViewer using the official SDK.

## Quick Start (5 Minutes)

### 1. Install LogViewer SDK

```bash
# Download or clone LogViewer
git clone https://github.com/yourname/LogViewer.git
cd LogViewer

# Build and install SDK
cmake -S . -B build -DBUILD_SDK=ON
cmake --install build --prefix ~/LogViewer_install
```

### 2. Build the Example Plugin

```bash
# Clone or navigate to the BasicPlugin example
cd examples/BasicPlugin

# Configure and build
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=~/LogViewer_install/sdk/lib/cmake
cmake --build build
```

### 3. Run Your Plugin

Copy to LogViewer's plugin directory and restart LogViewer:

```bash
# Linux
mkdir -p ~/.local/share/LogViewer/plugins
cp build/libbasic_example.so ~/.local/share/LogViewer/plugins/
cp build/config.json ~/.local/share/LogViewer/plugins/

# Windows
mkdir %APPDATA%\LogViewer\plugins
copy build\basic_example.dll %APPDATA%\LogViewer\plugins\
copy build\config.json %APPDATA%\LogViewer\plugins\
```

Restart LogViewer, and you should see the Basic Example plugin loaded.

---

## Detailed Guide

### What is the LogViewer Plugin SDK?

The SDK provides:
- **C-ABI Plugin Interface** (`PluginC.h`)
  - Cross-platform, stable ABI (no C++ version mismatch issues)
  - Supports all compilers and platforms

- **Header-Only Plugin API Library**
  - `LogViewer::plugin_api` CMake target
  - Pure headers, zero runtime dependency
  - Automatically includes all required dependencies via `find_package(LogViewer CONFIG)`

- **CMake Integration**
  - `LogViewerConfig.cmake` handles all dependency discovery
  - Plugin developers only need one `find_package()` call

- **Example and Documentation**
  - Basic Example plugin in `examples/BasicPlugin/`
  - Complete API headers with inline documentation

### SDK Installation

#### Build from Source

```bash
git clone <logviewer-repo>
cd LogViewer

# With SDK support
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SDK=ON

# Optional: limit builds to SDK only
cmake --build build --target install

# Install to custom location
cmake --install build --prefix ~/LogViewer_install
```

The SDK is installed to: `~/LogViewer_install/sdk/`

#### SDK Contents

```
sdk/
├── include/                    # Plugin API headers
│   ├── PluginC.h             # Main plugin interface
│   ├── PluginEventsC.h       # Event container access
│   ├── PluginHostUiC.h       # Host UI callback hooks
│   ├── PluginLoggerC.h       # Logger interface
│   ├── PluginTypesC.h        # Common types
│   ├── PluginAIProviderC.h   # AI provider interface (optional)
│   └── PluginKeyEncryptionC.h # Encryption interface (optional)
│
└── lib/cmake/
    ├── LogViewerConfig.cmake           # Main config file
    ├── LogViewerPluginTargets.cmake    # Target definitions
    ├── LogViewerPluginPackaging.cmake  # ZIP packaging helpers
    └── LogViewerConfigVersion.cmake    # Version info
```

### Creating Your First Plugin

#### Step 1: Minimal Plugin Structure

```
MyPlugin/
├── CMakeLists.txt
├── src/
│   └── MyPlugin.cpp
├── config.json.in
└── README.md
```

#### Step 2: Implement BasicPlugin.cpp

Minimum required exports:

```cpp
#include <PluginC.h>
#include <cstdlib>

extern "C" {

EXPORT_PLUGIN_SYMBOL
PluginHandle Plugin_Create() {
    return malloc(256);  // Simple state storage
}

EXPORT_PLUGIN_SYMBOL
void Plugin_Destroy(PluginHandle h) {
    free(h);
}

EXPORT_PLUGIN_SYMBOL
const char* Plugin_GetApiVersion(PluginHandle h) {
    return LOGVIEWER_PLUGIN_API_VERSION;
}

EXPORT_PLUGIN_SYMBOL
const char* Plugin_GetMetadataJson(PluginHandle h) {
    // Return malloc'd JSON
    const char* json = R"({"name":"MyPlugin","version":"1.0"})";
    size_t len = strlen(json);
    char* copy = (char*)malloc(len + 1);
    strcpy(copy, json);
    return copy;
}

EXPORT_PLUGIN_SYMBOL
bool Plugin_Initialize(PluginHandle h) {
    return true;  // Success
}

EXPORT_PLUGIN_SYMBOL
void Plugin_Shutdown(PluginHandle h) {}

EXPORT_PLUGIN_SYMBOL
const char* Plugin_GetLastError(PluginHandle h) {
    return strdup("No error");
}

EXPORT_PLUGIN_SYMBOL
void Plugin_SetLoggerCallback(PluginHandle h, PluginLogFn log) {}

}
```

#### Step 3: CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyPlugin)

# Find SDK - automatically provides Qt6, Threads, and optional libs
find_package(LogViewer CONFIG REQUIRED)

# Create plugin library
add_library(my_plugin MODULE src/MyPlugin.cpp)

# Link against plugin API
target_link_libraries(my_plugin PRIVATE LogViewer::plugin_api)

# Configure output
set_target_properties(my_plugin PROPERTIES PREFIX "")

# Generate config.json
configure_file(config.json.in config.json @ONLY)
```

#### Step 4: config.json.in

```json
{
  "id": "my_plugin",
  "name": "My Plugin",
  "version": "1.0.0",
  "entry": "my_plugin.dll",
  "api_version": "1.0.0",
  "description": "My awesome plugin",
  "capabilities": []
}
```

#### Step 5: Build

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/path/to/LogViewer_install/sdk/lib/cmake
cmake --build build
```

### Understanding the Plugin Lifecycle

```
┌─────────────────────────────────────────┐
│  Host Application (LogViewer)           │
└─────────────────────────────────────────┘
             ↓
    Plugin_Create()          [allocate state]
             ↓
    Plugin_SetLoggerCallback [receive logger]
             ↓
    Plugin_Initialize()      [setup, return true/false]
             ↓
         [Plugin active - can process events, create UI, etc.]
             ↓
    Plugin_Shutdown()        [cleanup, release resources]
             ↓
    Plugin_Destroy()         [deallocate state]
```

**Important**: Always use `malloc()` for any data returned to the host—the host will call `free()`.

### Using the Logger

In your plugin:

```cpp
static PluginLogFn g_logger = nullptr;

EXPORT_PLUGIN_SYMBOL
void Plugin_SetLoggerCallback(PluginHandle h, PluginLogFn log) {
    g_logger = log;
}

void my_function() {
    if (g_logger) {
        g_logger(0, "Debug message");      // level 0
        g_logger(1, "Info message");       // level 1
        g_logger(2, "Warning message");    // level 2
        g_logger(3, "Error message");      // level 3
    }
}
```

### Creating UI Panels

Implement any of these optional exports:

```cpp
EXPORT_PLUGIN_SYMBOL
void* Plugin_CreateLeftPanel(PluginHandle h, void* parent, const char* settings_json) {
    // parent is a QWidget*
    // Create and return a new QWidget* child
    QWidget* panel = new QWidget((QWidget*)parent);
    // ... setup panel ...
    return panel;  // Return as void*
}

// Similar functions:
// - Plugin_CreateRightPanel
// - Plugin_CreateBottomPanel
// - Plugin_CreateMainPanel

// Optional cleanup:
EXPORT_PLUGIN_SYMBOL
void Plugin_DestroyLeftPanel(PluginHandle h, void* widget) {
    delete (QWidget*)widget;
}
```

The host manages widget lifetimes but may call the Destroy function if provided.

### Accessing Events

Use the Events Container API:

```cpp
#include <PluginEventsC.h>

bool my_plugin_init(PluginHandle h) {
    // The host will call this during your initialization
    // Store the handle and functions for later use
    return true;
}

void analyze_events() {
    int count = PluginEvents_GetSize();
    for (int i = 0; i < count; ++i) {
        char* json = PluginEvents_GetEventJson(i);
        // Parse JSON and process event
        PluginApi_FreeString(json);
    }
}
```

### Dependencies

Automatically available after `find_package(LogViewer CONFIG REQUIRED)`:

**Always included:**
- Qt6 (Core, Widgets)
- Threads

**Available if built:**
- nlohmann_json (header-only JSON)
- fmt (modern string formatting)
- CURL (HTTP client)
- spdlog (fast logging)
- LibArchive (archive handling)
- EXPAT (XML parsing)

Use them in your plugin:

```cmake
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)

target_link_libraries(my_plugin
    PRIVATE
        LogViewer::plugin_api
        nlohmann_json::nlohmann_json
        fmt::fmt
)
```

### Platform Considerations

#### Windows (MinGW)

- Use forward slashes in paths: `C:/msys64/mingw64/`
- Plugin extension: `.dll`
- Set `CMAKE_PREFIX_PATH` with forward slashes

#### Linux

- Plugin extension: `.so`
- May need to link Qt plugins for GUI plugins
- Use standard CMake conventions

#### macOS

- Plugin extension: `.dylib`
- Qt6 typically installed via Homebrew
- Code signing may be required for distribution

### Distribution

Package plugins as ZIP archives:

```
my_plugin.zip
├── my_plugin.dll
├── config.json
└── lib/
    ├── Qt6Widgets.dll
    ├── Qt6Core.dll
    └── ... (runtime dependencies)
```

Use `LogViewerPluginPackaging.cmake` for automated packaging:

```cmake
include(LogViewerPluginPackaging)

# Bundles your plugin with dependencies into a ZIP
```

### Debugging

#### View Plugin Loads

LogViewer logs plugin loads to:
- **Windows**: `%APPDATA%/LogViewer/logs/`
- **Linux**: `~/.local/share/LogViewer/logs/`
- **macOS**: `~/Library/Logs/LogViewer/`

Check logs if your plugin fails to load.

#### Missing config.json

Ensure `config.json` is in the same directory as your plugin binary with:
- Correct `entry` field matching the binary name
- Valid JSON syntax
- Required fields: `id`, `name`, `version`, `entry`, `api_version`

#### Symbol Visibility

Use `EXPORT_PLUGIN_SYMBOL` for all exported functions:

```cpp
// WRONG - won't be visible
PluginHandle Plugin_Create() { ... }

// CORRECT
EXPORT_PLUGIN_SYMBOL
PluginHandle Plugin_Create() { ... }
```

### Best Practices

1. **Always use `malloc()` for returned strings**
   ```cpp
   char* get_string() {
       char* s = (char*)malloc(256);
       strcpy(s, "data");
       return s;  // Caller will free()
   }
   ```

2. **Store state in opaque handles**
   ```cpp
   struct MyState { ... };
   return (PluginHandle)new MyState();
   ```

3. **Check callbacks before using them**
   ```cpp
   if (g_logger) g_logger(0, "message");
   ```

4. **Implement proper error handling**
   ```cpp
   bool Plugin_Initialize(PluginHandle h) {
       if (/* error */) {
           last_error = "Reason for failure";
           return false;
       }
       return true;
   }
   ```

5. **Document your API in JSON**
   ```json
   {
     "name": "My Plugin",
     "capabilities": [
       "ui_panel_right",
       "event_analysis"
     ],
     "settings": {
       "api_key": "string"
     }
   }
   ```

## Examples

See `examples/BasicPlugin/` for a complete working example with detailed comments.

## API Reference

### Core Functions

| Function | Purpose |
|----------|---------|
| `Plugin_Create()` | Allocate plugin instance |
| `Plugin_Destroy(h)` | Deallocate plugin instance |
| `Plugin_GetApiVersion(h)` | Report SDK version |
| `Plugin_GetMetadataJson(h)` | Return metadata JSON |
| `Plugin_Initialize(h)` | Initialize after creation |
| `Plugin_Shutdown(h)` | Cleanup before destroy |
| `Plugin_GetLastError(h)` | Get last error message |
| `Plugin_SetLoggerCallback(h, fn)` | Receive logger function |

### Optional UI Functions

| Function | Purpose |
|----------|---------|
| `Plugin_CreateLeftPanel(h, parent, json)` | Create left panel widget |
| `Plugin_CreateRightPanel(h, parent, json)` | Create right panel widget |
| `Plugin_CreateBottomPanel(h, parent, json)` | Create bottom panel widget |
| `Plugin_CreateMainPanel(h, parent, json)` | Create main panel widget |

See [PluginC.h](../src/plugin_api/PluginC.h) for complete documentation.

## Next Steps

1. Build the [BasicPlugin example](examples/BasicPlugin/)
2. Modify it to add your own functionality
3. Create a UI panel using Qt6
4. Access log events via the Events API
5. Package and distribute your plugin

## Support

- **Documentation**: See `docs/` directory
- **Example**: `examples/BasicPlugin/`
- **Headers**: `src/plugin_api/`
- **Issues**: File issues on the repository

## License

Plugins built with this SDK are governed by the LogViewer license. See LICENSE file.
