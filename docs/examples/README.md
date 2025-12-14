# Plugin Examples

This directory contains example implementations demonstrating how to create plugins for LogViewer.

## ExampleParserPlugin

**Files**: `ExampleParserPlugin.hpp`, `ExampleParserPlugin.cpp`

Demonstrates how to create a parser plugin that implements both:
- `plugin::IPlugin` - Base plugin interface
- `parser::IDataParser` - Type-specific parser interface

### Key Concepts

1. **Dual Interface Implementation**: Parser plugins must implement both IPlugin and IDataParser
2. **Type-Specific Access**: Override `GetParserInterface()` to return `this`
3. **Extension Support**: Implement `SupportsExtension()` and `GetSupportedExtensions()`
4. **Observer Pattern**: Notify observers about parsing progress and new events
5. **License Management**: Implement license validation if required
6. **Error Handling**: Use `GetLastError()` to report errors

### Usage

To create your own parser plugin:

1. Copy `ExampleParserPlugin.hpp` and `ExampleParserPlugin.cpp`
2. Rename the class and namespace
3. Update metadata in `GetMetadata()`
4. Implement your parsing logic in `ParseStream()`
5. Update supported extensions in `GetSupportedExtensions()`
6. Build as shared library
7. Load via Plugin Manager or config dialog

### Building

Create a CMakeLists.txt:

```cmake
add_library(example_parser_plugin SHARED
    ExampleParserPlugin.cpp
    ExampleParserPlugin.hpp
)

target_link_libraries(example_parser_plugin
    PRIVATE
        application_core
)

set_target_properties(example_parser_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "example_parser_plugin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

install(TARGETS example_parser_plugin
    LIBRARY DESTINATION plugins
)
```

Then build:

```bash
cmake -B build
cmake --build build
```

The plugin will be in `build/plugins/example_parser_plugin.dylib` (or .so/.dll)

### Testing

1. Load plugin via config dialog:
   - Open Settings → Plugins tab
   - Click "Browse..." and select plugin file
   - Click "Load Plugin"

2. Load plugin programmatically:
   ```cpp
   auto& mgr = plugin::PluginManager::GetInstance();
   auto result = mgr.LoadPlugin("/path/to/example_parser_plugin.dylib");
   if (result.isOk()) {
       auto* plugin = mgr.GetPlugin("example_parser");
       auto* parser = plugin->GetParserInterface();
       // Use parser...
   }
   ```

### Custom Log Format

The example parser handles this format:

```
TIMESTAMP|LEVEL|MESSAGE
```

Example file (`.example` or `.exmpl` extension):

```
2025-12-07 10:00:00|INFO|Application started
2025-12-07 10:00:01|DEBUG|Loading configuration
2025-12-07 10:00:02|WARNING|Configuration file not found, using defaults
2025-12-07 10:00:03|ERROR|Failed to connect to database
2025-12-07 10:00:04|INFO|Retrying connection...
```

Lines starting with `#` are treated as comments.

## Creating Filter Plugins

Similar to parser plugins, filter plugins implement:
- `plugin::IPlugin` - Base plugin interface
- `filters::IFilterStrategy` - Type-specific filter interface

Key differences:
- Set `metadata.type = PluginType::Filter`
- Override `GetFilterInterface()` to return `this`
- Implement `Matches()` and other IFilterStrategy methods

## Creating Other Plugin Types

For Exporter, Analyzer, Connector, Visualizer, or Custom plugins:
- Implement `plugin::IPlugin` interface
- Set appropriate `metadata.type`
- Define your own type-specific methods
- Consumers cast to your specific plugin type

Example:
```cpp
class MyConnectorPlugin : public plugin::IPlugin {
public:
    // IPlugin methods...
    
    // Custom connector methods
    virtual bool Connect(const std::string& url) = 0;
    virtual void Disconnect() = 0;
};

// Usage:
auto* plugin = manager.GetPlugin("my_connector");
auto* connector = dynamic_cast<MyConnectorPlugin*>(plugin);
if (connector) {
    connector->Connect("tcp://192.168.1.1:5000");
}
```

## Best Practices

1. **Error Handling**: Always set `m_lastError` and return appropriate error codes
2. **Thread Safety**: Make plugins thread-safe if used from multiple threads
3. **Resource Cleanup**: Properly cleanup in `Shutdown()` method
4. **License Validation**: Implement proper license checking for commercial plugins
5. **Documentation**: Document plugin-specific methods and configuration
6. **Testing**: Create unit tests for plugin functionality
7. **Versioning**: Use semantic versioning (MAJOR.MINOR.PATCH)
8. **Dependencies**: Minimize dependencies on other plugins

## Support

For questions or issues:
- Check main documentation: `docs/PLUGIN_SYSTEM.md`
- Review implementation summary: `docs/PLUGIN_IMPLEMENTATION.md`
- Open an issue on GitHub
