# LogViewer Plugin SDK - Basic Example

This is a minimal but complete example plugin demonstrating how to build a plugin for LogViewer using the official SDK.

## What This Plugin Does

The Basic Example plugin demonstrates:
- Plugin lifecycle management (Create, Initialize, Shutdown, Destroy)
- Metadata reporting in JSON format
- Logger callback handling
- Plugin state management
- Proper C-ABI export macros for cross-platform compatibility

This plugin doesn't do anything functionally interesting—it's designed to be simple and educational so you can understand the SDK mechanics.

## Prerequisites

1. **LogViewer SDK Installed**
   - Install LogViewer or build it with SDK support:
     ```bash
     cd LogViewer
     cmake -S . -B build -DBUILD_SDK=ON
     cmake --install build --prefix /path/to/install
     ```

2. **CMake >= 3.22**
   - Required for modern CMake features

3. **C++ Compiler**
   - GCC, Clang, or MSVC with C++17 support

4. **Qt6 (optional for this example)**
   - The SDK will have Qt6 available if you built LogViewer from source

## Project Structure

```
BasicPlugin/
├── CMakeLists.txt           # Build configuration
├── config.json.in           # Plugin metadata template
├── README.md               # This file
└── src/
    └── BasicPlugin.cpp     # Plugin implementation
```

## Building the Plugin

### Step 1: Configure

Using the installed LogViewer SDK:

```bash
# On Linux/macOS
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/path/to/LogViewer_install/sdk/lib/cmake

# On Windows (MinGW in MSYS2)
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="C:/Users/YourName/LogViewer_install/sdk/lib/cmake"
```

Or if building as part of the LogViewer repository:

```bash
# In the LogViewer repository, the SDK path is:
cmake -S examples/BasicPlugin -B build \
  -DCMAKE_PREFIX_PATH="${PWD}/build/windows-release-qt/src/plugin_api"
```

### Step 2: Build

```bash
cmake --build build
```

On Windows/MinGW, you'll see output like:
```
[1/2] Building CXX object CMakeFiles/basic_example.dir/src/BasicPlugin.cpp.obj
[2/2] Linking CXX shared library basic_example.dll
```

### Step 3: Output

The built plugin will be:
- **Linux**: `build/libbasic_example.so` + `build/config.json`
- **macOS**: `build/libbasic_example.dylib` + `build/config.json`
- **Windows**: `build/basic_example.dll` + `build/config.json`

### Step 4: Plugin Package (ZIP Archive)

The plugin is automatically packaged as a **ZIP archive** for distribution:

```
build/package/basic_example.zip
├── basic_example.dll        (or .so/.dylib)
├── config.json
└── lib/                      (on Windows: runtime dependencies)
    ├── Qt6Core.dll
    ├── Qt6Gui.dll
    ├── libgcc_s_seh-1.dll
    └── ... other required DLLs
```

This ZIP file is the standard distribution format for LogViewer plugins. It includes all required runtime dependencies (DLLs on Windows), so users can extract it to the LogViewer plugins directory and the plugin will work immediately without additional setup.

## Plugin Code Structure

### C-ABI Export Macro

All exported functions use the `EXPORT_PLUGIN_SYMBOL` macro from `PluginC.h`:

```cpp
extern "C" {

EXPORT_PLUGIN_SYMBOL
PluginHandle Plugin_Create() {
    // Return opaque plugin instance handle
    return new PluginState();
}

// ... other exports
}
```

This ensures the plugin symbols are visible to the host application on all platforms.

### Required Plugin Exports

Every plugin MUST export these functions:

| Function | Purpose |
|----------|---------|
| `Plugin_Create()` | Instantiate the plugin |
| `Plugin_Destroy(handle)` | Clean up the instance |
| `Plugin_GetApiVersion(handle)` | Report API version for compatibility check |
| `Plugin_GetMetadataJson(handle)` | Return plugin metadata as JSON |
| `Plugin_Initialize(handle)` | Initialize after creation |
| `Plugin_Shutdown(handle)` | Clean up before destruction |
| `Plugin_GetLastError(handle)` | Return last error message if init failed |

### Optional Plugin Exports

Plugins may also export these to provide UI panels:

- `Plugin_CreateLeftPanel(handle, parent, settings_json)` → QWidget*
- `Plugin_CreateRightPanel(handle, parent, settings_json)` → QWidget*
- `Plugin_CreateBottomPanel(handle, parent, settings_json)` → QWidget*
- `Plugin_CreateMainPanel(handle, parent, settings_json)` → QWidget*

See [PluginC.h](../../src/plugin_api/PluginC.h) for details.

## Using the SDK to Build Your Plugin

### CMakeLists.txt Pattern

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyPlugin)

# Find the LogViewer SDK
find_package(LogViewer CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)

# Create your plugin library
add_library(my_plugin MODULE src/MyPlugin.cpp)

# Link the plugin API (header-only)
target_link_libraries(my_plugin
    PRIVATE
        LogViewer::plugin_api
        Qt6::Widgets
)

# Configure for your platform
set_target_properties(my_plugin PROPERTIES PREFIX "")
```

### What find_package(LogViewer) Provides

When you call `find_package(LogViewer CONFIG REQUIRED)`, you automatically get:

**Required (auto-discovered):**
- Qt6 (Core, Widgets)
- Threads

**Optional (available if built):**
- nlohmann_json
- fmt
- CURL
- spdlog
- LibArchive
- EXPAT

No need for redundant `find_package()` calls for these dependencies!

## Deploying Your Plugin

### Plugin Package (Recommended for Distribution)

The build automatically creates a **ZIP archive** containing your plugin:

```bash
# Location: build/package/basic_example.zip
# Contents:
#   - basic_example.dll (or .so/.dylib)
#   - config.json
```

**To distribute your plugin:**
1. Share the `build/package/basic_example.zip` file
2. Users extract it to their LogViewer plugins directory
3. Restart LogViewer to load the plugin

### Manual Installation to Plugin Directory

For development/testing, copy files directly:

Plugins are loaded from:
- **Windows**: `%APPDATA%/LogViewer/plugins/`
- **Linux**: `~/.local/share/LogViewer/plugins/`
- **macOS**: `~/Library/Application Support/LogViewer/plugins/`

```bash
# Extract plugin ZIP and copy to plugins directory
unzip -d ~/.local/share/LogViewer/plugins/ build/package/basic_example.zip

# OR copy individual files
mkdir -p ~/.local/share/LogViewer/plugins/basic_example
cp build/libbasic_example.so ~/.local/share/LogViewer/plugins/basic_example/
cp build/config.json ~/.local/share/LogViewer/plugins/basic_example/
```

Restart LogViewer to load the plugin.

### Plugin Package Structure

Plugins distributed as ZIP archives contain:

```
basic_example.zip
├── basic_example.dll          # Plugin binary
├── config.json               # Plugin metadata
└── lib/                       # Runtime dependencies (Windows/Linux)
    ├── Qt6Core.dll
    ├── Qt6Gui.dll
    ├── libstdc++-6.dll        (MinGW on Windows)
    └── ... all other required DLLs
```

**Dependency Collection:** The build automatically scans your plugin binary for required libraries and includes them in the `lib/` directory. This ensures the plugin is **self-contained** and will work on end-user systems without requiring them to install additional dependencies.

On deployment, users simply extract the ZIP archive—all dependencies are included and in the correct relative paths.

## Debugging Your Plugin

### Logger Callback

Your plugin receives a logger function during initialization:

```cpp
void Plugin_SetLoggerCallback(PluginHandle handle, PluginLogFn logger) {
    g_logger = logger;
}
```

Use it to log messages:

```cpp
log_message(0, "BasicPlugin: Message level 0");
log_message(1, "BasicPlugin: Message level 1");
```

The host application controls where these messages appear (console, file, etc.).

### Error Handling

If `Plugin_Initialize()` returns `false`, the host calls `Plugin_GetLastError()` to retrieve the error message:

```cpp
bool Plugin_Initialize(PluginHandle handle) {
    if (initialization_failed) {
        last_error = "Initialization failed because...";
        return false;
    }
    return true;
}

const char* Plugin_GetLastError(PluginHandle handle) {
    char* copy = (char*)malloc(strlen(last_error) + 1);
    strcpy(copy, last_error);
    return copy;  // Caller will free() this
}
```

## Next Steps

1. **Read the API Headers**: Check [PluginC.h](../../src/plugin_api/PluginC.h) and [PluginEventsC.h](../../src/plugin_api/PluginEventsC.h) for the complete interface.

2. **Create a UI Panel**: Extend the plugin to create a Qt widget panel:
   - Implement `Plugin_CreateRightPanel()`
   - Return a `QWidget*` (opaque to the host)
   - The host will parent and manage its lifetime

3. **Access Events**: Use the Events Container API to read log events:
   ```cpp
   PluginEvents_Register(handle, getSizeFn, getEventFn);
   int count = PluginEvents_GetSize();
   char* json = PluginEvents_GetEventJson(0);
   ```

4. **Build as Part of LogViewer**: Add your plugin to `plugins/` directory in the LogViewer repo and update `plugins/CMakeLists.txt`:
   ```cmake
   add_subdirectory(my_plugin)
   ```

## Troubleshooting

### CMake can't find LogViewer

**Error**: `Could not find a package configuration file provided by "LogViewer"`

**Solution**: Set `CMAKE_PREFIX_PATH` to the SDK's CMake config directory:
```bash
cmake -DCMAKE_PREFIX_PATH=/path/to/LogViewer_install/sdk/lib/cmake
```

### Qt6 not found

**Error**: `Could not find a package configuration file provided by "Qt6"`

**Solution**: This is handled by `LogViewerConfig.cmake`. Ensure the LogViewer SDK was built with Qt6 support.

### Plugin fails to load

1. Check `config.json` exists in the same directory as the plugin DLL
2. Verify the `entry` field in `config.json` matches the actual plugin filename
3. Check LogViewer logs for detailed error messages

## License

This example is part of LogViewer. See [LICENSE](../../license.md) for details.

## See Also

- [Plugin System Architecture](../../docs/PLUGIN_SYSTEM.md)
- [Plugin Implementation Guide](../../docs/PLUGIN_IMPLEMENTATION.md)
- [LogViewer Build Documentation](../../docs/BUILDING_DOCUMENTATION.md)
