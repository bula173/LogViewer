# LogViewer Plugin System

## Overview

LogViewer supports a plugin architecture that allows extending functionality without modifying the core application. Plugins are dynamically loaded shared libraries that implement the `IPlugin` interface.

## Plugin Types

The system supports several plugin types:

- **Parser**: Custom log file format parsers (must also implement `parser::IDataParser`)
- **Filter**: Custom filtering strategies (must also implement `filters::IFilterStrategy`)
- **Exporter**: Export logs to different formats
- **Analyzer**: Advanced log analysis tools
- **Connector**: Remote log source connections (e.g., SSH, databases)
- **Visualizer**: Custom visualization components
- **Custom**: Generic plugins for other purposes

## Plugin Architecture

### IPlugin Interface

All plugins must implement the `IPlugin` interface defined in `src/application/plugins/IPlugin.hpp`:

```cpp
class IPlugin {
public:
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual PluginMetadata GetMetadata() const = 0;
    virtual PluginStatus GetStatus() const = 0;
    virtual std::string GetLastError() const = 0;
    virtual bool IsLicensed() const = 0;
    virtual bool SetLicense(const std::string& licenseKey) = 0;
    
    // Type-specific interface access
    virtual parser::IDataParser* GetParserInterface() { return nullptr; }
    virtual filters::IFilterStrategy* GetFilterInterface() { return nullptr; }
    
    // For Parser plugins
    virtual bool SupportsExtension(const std::string& extension) const { return false; }
    virtual std::vector<std::string> GetSupportedExtensions() const { return {}; }
};
```

### Type-Specific Interfaces

Plugins must implement additional type-specific interfaces based on their type:

#### Parser Plugins

Parser plugins **MUST** implement both:
1. `plugin::IPlugin` - Base plugin interface
2. `parser::IDataParser` - Parser-specific interface

Example:
```cpp
class MyParserPlugin : public plugin::IPlugin, public parser::IDataParser
{
public:
    // IPlugin methods...
    parser::IDataParser* GetParserInterface() override { return this; }
    bool SupportsExtension(const std::string& ext) const override {
        return ext == ".myformat";
    }
    
    // IDataParser methods...
    void Parse(const std::filesystem::path& path) override { /* ... */ }
    void ParseStream(std::istream& stream) override { /* ... */ }
    // ... other IDataParser methods
};
```

#### Filter Plugins

Filter plugins **MUST** implement both:
1. `plugin::IPlugin` - Base plugin interface
2. `filters::IFilterStrategy` - Filter-specific interface

Example:
```cpp
class MyFilterPlugin : public plugin::IPlugin, public filters::IFilterStrategy
{
public:
    // IPlugin methods...
    filters::IFilterStrategy* GetFilterInterface() override { return this; }
    
    // IFilterStrategy methods...
    bool Matches(const db::LogEvent& event) const override { /* ... */ }
    // ... other IFilterStrategy methods
};
```

See `docs/examples/ExampleParserPlugin.hpp` for a complete parser plugin implementation.

### Plugin Metadata

Each plugin provides metadata describing its capabilities:

```cpp
struct PluginMetadata {
    std::string id;              // Unique identifier
    std::string name;            // Display name
    std::string version;         // Version string
    std::string author;          // Author information
    std::string description;     // Plugin description
    std::string website;         // Plugin website/repository
    PluginType type;             // Plugin type
    bool requiresLicense;        // Whether license is required
    std::vector<std::string> dependencies;  // Other plugin dependencies
};
```

### Plugin Factory

Each plugin must export a factory function:

```cpp
extern "C" EXPORT_PLUGIN std::unique_ptr<IPlugin> CreatePlugin();
```

The `EXPORT_PLUGIN` macro ensures proper symbol visibility across platforms.

## Plugin Manager

The `PluginManager` singleton handles plugin lifecycle:

- **Discovery**: Scan plugins directory for available plugins
- **Loading**: Dynamically load plugin shared libraries
- **Registration**: Register plugins and validate dependencies
- **Lifecycle**: Initialize, enable, disable, and shutdown plugins
- **Configuration**: Persist plugin settings

### Usage Example

```cpp
// Get plugin manager instance
auto& manager = plugin::PluginManager::GetInstance();

// Set plugins directory
manager.SetPluginsDirectory("/path/to/plugins");

// Discover available plugins
auto plugins = manager.DiscoverPlugins();

// Load a specific plugin
auto result = manager.LoadPlugin("/path/to/plugin.dylib");
if (result.isOk()) {
    std::string pluginId = result.unwrap();
    
    // Get plugin instance
    auto* plugin = manager.GetPlugin(pluginId);
    
    // Set license if required
    if (plugin->GetMetadata().requiresLicense) {
        plugin->SetLicense("YOUR-LICENSE-KEY");
    }
}

// Enable/disable plugins
manager.EnablePlugin("plugin_id");
manager.DisablePlugin("plugin_id");

// Unload plugin
manager.UnloadPlugin("plugin_id");
```

## Creating a Plugin

### 1. Implement IPlugin Interface

Create a class that implements `IPlugin`:

```cpp
class MyPlugin : public plugin::IPlugin {
public:
    bool Initialize() override {
        // Initialize plugin resources
        return true;
    }
    
    void Shutdown() override {
        // Cleanup resources
    }
    
    plugin::PluginMetadata GetMetadata() const override {
        plugin::PluginMetadata metadata;
        metadata.id = "my_plugin";
        metadata.name = "My Plugin";
        metadata.version = "1.0.0";
        metadata.type = plugin::PluginType::Custom;
        return metadata;
    }
    
    // Implement other interface methods...
};
```

### 2. Export Factory Function

Export the factory function:

```cpp
extern "C" EXPORT_PLUGIN std::unique_ptr<plugin::IPlugin> CreatePlugin() {
    return std::make_unique<MyPlugin>();
}
```

### 3. Build as Shared Library

Create a CMakeLists.txt that builds your plugin as a shared library:

```cmake
add_library(my_plugin SHARED
    MyPlugin.cpp
    MyPlugin.hpp
)

target_link_libraries(my_plugin
    PRIVATE
        application_core
)

set_target_properties(my_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "my_plugin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

install(TARGETS my_plugin
    LIBRARY DESTINATION plugins
)
```

### 4. Enable in CMake

Add a CMake option to conditionally build your plugin:

```cmake
option(ENABLE_MY_PLUGIN "Enable My Plugin" OFF)

if(ENABLE_MY_PLUGIN)
    add_subdirectory(my_plugin)
endif()
```

## Creating Plugins Outside the Build System

If you want to develop plugins independently of the LogViewer build system, you can build them as standalone shared libraries. This approach requires you to manually manage dependencies and include paths.

### Prerequisites

Before building external plugins, ensure you have:

1. **LogViewer Source Access**: Access to LogViewer header files
2. **Compatible Compiler**: Same compiler and standard library version as LogViewer
3. **Required Dependencies**: 
   - nlohmann_json (header-only)
   - spdlog (header-only) 
   - expat (XML parsing)
   - CURL (HTTP requests)
   - C++17 compatible standard library

### Directory Structure

Organize your plugin project like this:

```
my_plugin/
├── CMakeLists.txt          # Standalone build configuration
├── MyPlugin.cpp           # Plugin implementation
├── MyPlugin.hpp           # Plugin header (optional)
└── build/                 # Build output directory
```

### CMakeLists.txt for External Plugin

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyPlugin VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required dependencies
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(EXPAT REQUIRED expat)

# Set LogViewer source paths (adjust these to match your LogViewer installation)
set(LOGVIEWER_SRC_DIR "/path/to/LogViewer/src")
set(LOGVIEWER_INCLUDE_DIR "/path/to/LogViewer/include")
set(LOGVIEWER_THIRDPARTY_DIR "/path/to/LogViewer/thirdparty")

# Create plugin library
add_library(my_plugin SHARED
    MyPlugin.cpp
)

# Include directories
target_include_directories(my_plugin PRIVATE
    ${LOGVIEWER_SRC_DIR}
    ${LOGVIEWER_INCLUDE_DIR}
    ${LOGVIEWER_THIRDPARTY_DIR}/nlohmann_json/include
    ${LOGVIEWER_THIRDPARTY_DIR}/spdlog/include
    ${EXPAT_INCLUDE_DIRS}
)

# Link dependencies
target_link_libraries(my_plugin PRIVATE
    CURL::libcurl
    ${EXPAT_LIBRARIES}
)

# Set library properties
set_target_properties(my_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "my_plugin"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Compiler flags (match LogViewer's build settings)
target_compile_options(my_plugin PRIVATE
    -Wall
    -Wextra
    -fPIC
)

# Installation
install(TARGETS my_plugin
    LIBRARY DESTINATION plugins
)
```

### Makefile Alternative

If you prefer using Make directly:

```makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -fPIC -shared
INCLUDES = -I/path/to/LogViewer/src \
           -I/path/to/LogViewer/include \
           -I/path/to/LogViewer/thirdparty/nlohmann_json/include \
           -I/path/to/LogViewer/thirdparty/spdlog/include \
           -I/usr/include/expat
LIBS = -lcurl -lexpat

TARGET = my_plugin.so
SOURCES = MyPlugin.cpp
OBJECTS = $(SOURCES:.cpp=.o)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /path/to/plugins/
```

### Visual Studio Project (Windows)

For Windows development, create a new DLL project with these settings:

1. **Project Type**: Dynamic Link Library (DLL)
2. **Include Directories**:
   - `C:\path\to\LogViewer\src`
   - `C:\path\to\LogViewer\include`
   - `C:\path\to\LogViewer\thirdparty\nlohmann_json\include`
   - `C:\path\to\LogViewer\thirdparty\spdlog\include`
3. **Library Directories**: Add paths to expat and CURL libraries
4. **Additional Dependencies**: `libcurl.lib`, `expat.lib`
5. **Preprocessor Definitions**: Add `EXPORT_PLUGIN_SYMBOL=__declspec(dllexport)`
6. **C++ Standard**: C++17 or later

### Important Considerations

#### Header Dependencies

Your plugin must include these key headers:

```cpp
#include "application/plugins/IPlugin.hpp"           // Base plugin interface
#include "application/config/IFieldConversionPlugin.hpp"  // For field conversion plugins
// Include other type-specific interfaces as needed
```

#### Library Compatibility

- **ABI Compatibility**: Ensure your plugin is built with the same compiler and standard library as LogViewer
- **Dependency Versions**: Use the same versions of third-party libraries (nlohmann_json, spdlog, etc.)
- **Platform Architecture**: Match the target platform (32-bit vs 64-bit)

#### Symbol Visibility

- Use the `EXPORT_PLUGIN` macro to ensure `CreatePlugin` is exported
- On Linux/macOS, ensure `-fPIC` is used when compiling
- On Windows, use `__declspec(dllexport)` for the factory function

#### Testing Your Plugin

1. Build your plugin as a shared library
2. Copy it to LogViewer's plugins directory
3. Start LogViewer and check if the plugin loads
4. Verify plugin functionality through the UI

#### Troubleshooting

**Plugin fails to load:**
- Check that all dependencies are available
- Verify `CreatePlugin` symbol is exported: `nm -g plugin.so | grep CreatePlugin`
- Ensure ABI compatibility with LogViewer

**Missing headers:**
- Verify include paths point to correct LogViewer source directories
- Check that third-party headers are accessible

**Linker errors:**
- Ensure all required libraries are linked
- Check library paths and versions match LogViewer build

## Available Plugins

### SSH Parser Plugin

**Location**: `src/plugins/ssh_parser/`

Provides SSH connectivity and remote log parsing capabilities.

**Features**:
- SSH connection with password, key, or agent authentication
- Text log parsing with regex patterns
- Real-time log monitoring (command output, file tailing, shell)

**Build Option**: `ENABLE_SSH_PARSER=ON`

**Dependencies**: libssh

**Usage**:
```cpp
auto& manager = plugin::PluginManager::GetInstance();
auto* plugin = dynamic_cast<ssh::SSHParserPlugin*>(
    manager.GetPlugin("ssh_parser"));

if (plugin && plugin->IsLicensed()) {
    auto connection = plugin->CreateConnection("hostname", 22, "user");
    auto parser = plugin->CreateTextParser();
    auto logSource = plugin->CreateLogSource(
        std::move(connection),
        std::move(parser),
        ssh::SSHLogSource::MonitorMode::CommandOutput);
}
```

## Plugin Configuration

Plugins can be configured through the application's configuration dialog:

1. **Plugin List**: View all discovered and loaded plugins
2. **Enable/Disable**: Toggle plugin activation
3. **Auto-Load**: Configure plugins to load automatically on startup
4. **License Management**: Set license keys for commercial plugins
5. **Dependencies**: View plugin dependencies and status

## Plugin Directory Structure

```
LogViewer/
├── src/
│   ├── application/
│   │   └── plugins/
│   │       ├── IPlugin.hpp          # Plugin interface
│   │       ├── PluginManager.hpp    # Plugin manager
│   │       └── PluginManager.cpp
│   └── plugins/
│       ├── CMakeLists.txt
│       └── ssh_parser/              # SSH parser plugin
│           ├── CMakeLists.txt
│           ├── SSHParserPlugin.hpp
│           ├── SSHParserPlugin.cpp
│           ├── SSHConnection.hpp
│           ├── SSHConnection.cpp
│           ├── SSHTextParser.hpp
│           ├── SSHTextParser.cpp
│           ├── SSHLogSource.hpp
│           └── SSHLogSource.cpp
└── build/
    └── plugins/                      # Built plugins
        └── ssh_parser_plugin.dylib
```

## Platform-Specific Notes

### macOS
- Plugin extension: `.dylib`
- Use `dlopen()` for dynamic loading
- Plugins built with: `add_library(plugin SHARED)`

### Linux
- Plugin extension: `.so`
- Use `dlopen()` for dynamic loading
- May require `-fPIC` compiler flag

### Windows
- Plugin extension: `.dll`
- Use `LoadLibrary()` for dynamic loading
- Requires proper `__declspec(dllexport)` decoration

## Troubleshooting

### Plugin Not Loading

1. Check plugin file exists in plugins directory
2. Verify plugin exports `CreatePlugin` function
3. Check dependencies are satisfied
4. Review error messages from `GetLastError()`

### License Issues

1. Verify license key is set: `plugin->SetLicense(key)`
2. Check `IsLicensed()` returns true
3. Contact plugin vendor for license support

### Dependency Errors

1. Ensure all required plugins are loaded first
2. Use `ValidateDependencies()` to check
3. Load plugins in dependency order

## Best Practices

1. **Error Handling**: Always check return values and handle errors
2. **Licensing**: Implement proper license validation for commercial plugins
3. **Dependencies**: Minimize dependencies between plugins
4. **Versioning**: Use semantic versioning for plugins
5. **Documentation**: Provide clear documentation for plugin usage
6. **Testing**: Create unit tests for plugin functionality
7. **Thread Safety**: Ensure plugins are thread-safe if used concurrently

## Future Enhancements

- Plugin hot-reloading support
- Plugin marketplace/repository
- Automatic dependency resolution
- Plugin sandboxing for security
- Plugin API versioning
- Remote plugin updates
