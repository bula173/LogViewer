# LogViewer SDK Quick Reference

Quick reference for plugin developers building LogViewer plugins.

## Building Your First Plugin (10 Minutes)

### 1. Prepare SDK

```bash
cd LogViewer
cmake -S . -B build -DBUILD_SDK=ON
cmake --install build --prefix ~/LogViewer_install
```

### 2. Create Plugin Project

```bash
mkdir MyPlugin && cd MyPlugin
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.22)
project(MyPlugin)

find_package(LogViewer CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core REQUIRED)

add_library(my_plugin MODULE src/MyPlugin.cpp)
target_link_libraries(my_plugin PRIVATE LogViewer::plugin_api Qt6::Core)
set_target_properties(my_plugin PROPERTIES PREFIX "")
EOF
```

### 3. Create Plugin Code

```bash
mkdir src
cat > src/MyPlugin.cpp << 'EOF'
#include <PluginC.h>
#include <cstdlib>
#include <cstring>

extern "C" {

EXPORT_PLUGIN_SYMBOL
PluginHandle Plugin_Create() {
    return malloc(1);
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
    const char* json = R"({"name":"MyPlugin","version":"1.0"})";
    char* copy = (char*)malloc(strlen(json) + 1);
    strcpy(copy, json);
    return copy;
}

EXPORT_PLUGIN_SYMBOL
bool Plugin_Initialize(PluginHandle h) {
    return true;
}

EXPORT_PLUGIN_SYMBOL
void Plugin_Shutdown(PluginHandle h) {}

EXPORT_PLUGIN_SYMBOL
const char* Plugin_GetLastError(PluginHandle h) {
    char* s = (char*)malloc(10);
    strcpy(s, "No error");
    return s;
}

EXPORT_PLUGIN_SYMBOL
void Plugin_SetLoggerCallback(PluginHandle h, PluginLogFn log) {}

}
EOF
```

### 4. Build

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=~/LogViewer_install/sdk/lib/cmake
cmake --build build
```

Done! Your plugin is in `build/libmy_plugin.so` (Linux) or `build/my_plugin.dll` (Windows).

---

## Required Plugin Exports

Every plugin MUST export these C-ABI functions:

| Function | Signature | Purpose |
|----------|-----------|---------|
| `Plugin_Create` | `PluginHandle()` | Allocate plugin instance |
| `Plugin_Destroy` | `void(PluginHandle)` | Deallocate instance |
| `Plugin_GetApiVersion` | `const char*(PluginHandle)` | Report SDK version |
| `Plugin_GetMetadataJson` | `const char*(PluginHandle)` | Return malloc'd JSON metadata |
| `Plugin_Initialize` | `bool(PluginHandle)` | Initialize after creation |
| `Plugin_Shutdown` | `void(PluginHandle)` | Cleanup before destroy |
| `Plugin_GetLastError` | `const char*(PluginHandle)` | Return malloc'd error message |
| `Plugin_SetLoggerCallback` | `void(PluginHandle, PluginLogFn)` | Receive logger function |

## Optional Plugin Exports

Plugins can optionally export UI creation functions:

```cpp
// These return QWidget* cast to void*
EXPORT_PLUGIN_SYMBOL
void* Plugin_CreateLeftPanel(PluginHandle h, void* parent, const char* json) { ... }

EXPORT_PLUGIN_SYMBOL
void* Plugin_CreateRightPanel(PluginHandle h, void* parent, const char* json) { ... }

EXPORT_PLUGIN_SYMBOL
void* Plugin_CreateBottomPanel(PluginHandle h, void* parent, const char* json) { ... }

EXPORT_PLUGIN_SYMBOL
void* Plugin_CreateMainPanel(PluginHandle h, void* parent, const char* json) { ... }

// Optional cleanup
EXPORT_PLUGIN_SYMBOL
void Plugin_DestroyLeftPanel(PluginHandle h, void* widget) { ... }
```

## Common Patterns

### Logger

```cpp
static PluginLogFn g_logger = nullptr;

void Plugin_SetLoggerCallback(PluginHandle h, PluginLogFn log) {
    g_logger = log;
}

void log_info(const char* msg) {
    if (g_logger) g_logger(0, msg);
}
```

### State Management

```cpp
struct PluginState {
    int value;
    bool initialized;
};

PluginHandle Plugin_Create() {
    return new PluginState();
}

void Plugin_Destroy(PluginHandle h) {
    delete (PluginState*)h;
}

bool Plugin_Initialize(PluginHandle h) {
    PluginState* state = (PluginState*)h;
    state->initialized = true;
    return true;
}
```

### String Returns (Malloc Pattern)

```cpp
const char* Plugin_GetMetadataJson(PluginHandle h) {
    const char* data = R"({"name":"MyPlugin"})";
    
    // MUST malloc - caller will free()
    char* copy = (char*)malloc(strlen(data) + 1);
    strcpy(copy, data);
    return copy;
}
```

## CMakeLists.txt Template

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyPlugin)

# Required
find_package(LogViewer CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

# Your plugin library
add_library(my_plugin MODULE src/MyPlugin.cpp)

# Link API
target_link_libraries(my_plugin
    PRIVATE
        LogViewer::plugin_api
        Qt6::Widgets
)

# Optional: Link other available packages
# find_package(nlohmann_json REQUIRED)
# find_package(fmt REQUIRED)
# find_package(CURL REQUIRED)
# find_package(spdlog REQUIRED)

# set_target_properties(my_plugin PROPERTIES PREFIX "")  # No "lib" prefix

# Generate config.json
set(PLUGIN_NAME "my_plugin")
set(PLUGIN_VERSION "1.0.0")
set(PLUGIN_ID "my_plugin")
set(PLUGIN_DISPLAY_NAME "My Plugin")
set(PLUGIN_API_VERSION "1.0.0")

if(WIN32)
    set(PLUGIN_EXTENSION ".dll")
elseif(APPLE)
    set(PLUGIN_EXTENSION ".dylib")
else()
    set(PLUGIN_EXTENSION ".so")
endif()

set(PLUGIN_ENTRY "my_plugin${PLUGIN_EXTENSION}")

configure_file(config.json.in config.json @ONLY)

add_custom_command(TARGET my_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/config.json
        $<TARGET_FILE_DIR:my_plugin>/config.json
)
```

## config.json Template

```json
{
  "id": "my_plugin",
  "name": "My Plugin",
  "version": "1.0.0",
  "entry": "my_plugin.dll",
  "api_version": "1.0.0",
  "description": "My awesome plugin",
  "author": "Your Name",
  "capabilities": [
    "ui_panel_right"
  ]
}
```

## Available Dependencies After find_package(LogViewer)

**Always included:**
- Qt6 (Core, Widgets, Gui, etc.)
- Threads

**Available if built:**
- nlohmann_json (JSON)
- fmt (formatting)
- CURL (HTTP)
- spdlog (logging)
- LibArchive (archives)
- EXPAT (XML)

Use them:

```cmake
find_package(nlohmann_json REQUIRED)
target_link_libraries(my_plugin PRIVATE nlohmann_json::nlohmann_json)
```

## Installing Your Plugin

### Plugin Distribution (Recommended)

Plugins are distributed as **ZIP archives** containing the binary, metadata, and all required dependencies:

```bash
# Build creates: build/package/my_plugin.zip
# Contents:
#   - my_plugin.dll (or .so/.dylib)
#   - config.json
#   - lib/                      (Windows: runtime dependencies)
#       ├── Qt6Core.dll
#       ├── Qt6Gui.dll
#       ├── libgcc_s_seh-1.dll
#       └── ... all required DLLs
```

**For users to install:**
```bash
# Extract to LogViewer plugins directory (dependencies are included!)
unzip my_plugin.zip -d ~/.local/share/LogViewer/plugins/
```

**To create a ZIP archive with dependency collection:**

Use the [BasicPlugin example](../examples/BasicPlugin/CMakeLists.txt) as a template. It automatically:
1. Scans the plugin binary for required dependencies using `copy_dlls.cmake` (provided by LogViewer SDK)
2. Collects DLLs to a `lib/` directory
3. Packages everything into a self-contained ZIP file

```cmake
# Simplified example from BasicPlugin/CMakeLists.txt
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")

# Collect runtime dependencies (Windows)
# copy_dlls.cmake is provided by the LogViewer SDK
if(WIN32)
    get_filename_component(LOGVIEWER_CMAKE_DIR "${LogViewer_CONFIG}" DIRECTORY)
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DARGV0="$<TARGET_FILE:${PLUGIN_NAME}>"
            -DARGV1="${CMAKE_CURRENT_BINARY_DIR}/lib"
            -DARGV2="${CMAKE_C_COMPILER}"
            -P "${LOGVIEWER_CMAKE_DIR}/copy_dlls.cmake"
    )
endif()

# Create ZIP with all dependencies
add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cfz" "${PLUGIN_PACKAGE_PATH}"
        --format=zip
        "$<TARGET_FILE:${PLUGIN_NAME}>"
        "${CMAKE_CURRENT_BINARY_DIR}/config.json"
        "lib/"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
```

This ensures your plugin is **self-contained** and works on end-user systems without additional dependency installation.

### Manual Installation

For development/testing, copy files directly:

```bash
# Linux
mkdir -p ~/.local/share/LogViewer/plugins
cp build/libmy_plugin.so ~/.local/share/LogViewer/plugins/
cp build/config.json ~/.local/share/LogViewer/plugins/

# Windows
mkdir %APPDATA%\LogViewer\plugins
copy build\my_plugin.dll %APPDATA%\LogViewer\plugins\
copy build\config.json %APPDATA%\LogViewer\plugins\

# macOS
mkdir -p ~/Library/Application\ Support/LogViewer/plugins
cp build/libmy_plugin.dylib ~/Library/Application\ Support/LogViewer/plugins/
cp build/config.json ~/Library/Application\ Support/LogViewer/plugins/
```

## Common Errors

| Error | Solution |
|-------|----------|
| `Could not find LogViewer` | Set `-DCMAKE_PREFIX_PATH=/path/to/sdk/lib/cmake` |
| `Cannot open PluginC.h` | Ensure `find_package(LogViewer CONFIG REQUIRED)` is in CMakeLists |
| `Plugin won't load` | Check config.json exists and `entry` field matches binary name |
| `Symbol not found` | Use `EXPORT_PLUGIN_SYMBOL` on all exported functions |
| `Undefined reference` | Link against `LogViewer::plugin_api` |

## Headers Reference

| Header | Purpose |
|--------|---------|
| `PluginC.h` | Main plugin interface (REQUIRED) |
| `PluginEventsC.h` | Access log events |
| `PluginHostUiC.h` | Host UI callback hooks |
| `PluginLoggerC.h` | Logger interface |
| `PluginTypesC.h` | Common type definitions |
| `PluginAIProviderC.h` | AI provider interface |
| `PluginKeyEncryptionC.h` | Key encryption interface |

## Example

See `examples/BasicPlugin/` for a complete working example:

```bash
cd examples/BasicPlugin
cmake -S . -B build -DCMAKE_PREFIX_PATH=~/LogViewer_install/sdk/lib/cmake
cmake --build build
```

## Useful Links

- [SDK Getting Started Guide](SDK_GETTING_STARTED.md)
- [Plugin Implementation Guide](PLUGIN_IMPLEMENTATION.md)
- [Plugin Architecture](ARCHITECTURE.md)
- [API Headers](../src/plugin_api/)

## Build with LogViewer

Add your plugin to the LogViewer repo:

```bash
# 1. Copy your plugin to plugins/ directory
cp -r MyPlugin LogViewer/plugins/

# 2. Add to plugins/CMakeLists.txt:
add_subdirectory(MyPlugin)

# 3. Build LogViewer with plugins
cmake -S LogViewer -B build -DBUILD_PLUGINS=ON
cmake --build build
```

## Tips

1. **Always use `EXPORT_PLUGIN_SYMBOL`** on exported functions
2. **Always `malloc()` strings returned to host** - the host will `free()`
3. **Store state in plugin handle** - don't use globals
4. **Check logger before using** - it might be nullptr initially
5. **Put `config.json` next to binary** - required for loading
6. **Test with the BasicPlugin example first** - minimal overhead to understand

---

**For detailed documentation, see:**
- [SDK Getting Started](SDK_GETTING_STARTED.md)
- [Basic Plugin Example](../examples/BasicPlugin/README.md)
- [Plugin Implementation Guide](PLUGIN_IMPLEMENTATION.md)
