# Plugin System: Type-Specific Interfaces Implementation

## Summary

Updated the plugin system to support type-specific interfaces. Plugins now must implement both the base `IPlugin` interface and their type-specific interface (e.g., `IDataParser` for parser plugins, `IFilterStrategy` for filter plugins).

## Changes Made

### 1. Enhanced IPlugin Interface

**File**: `src/application/plugins/IPlugin.hpp`

**Added**:
- Forward declarations for type-specific interfaces (`parser::IDataParser`, `filters::IFilterStrategy`)
- `GetParserInterface()` - Returns IDataParser interface for Parser plugins
- `GetFilterInterface()` - Returns IFilterStrategy interface for Filter plugins  
- `SupportsExtension()` - Check if plugin supports a file extension
- `GetSupportedExtensions()` - Get list of supported extensions

**Benefits**:
- Type-safe access to parser/filter functionality
- Automatic file extension detection for parsers
- Cleaner plugin integration with existing systems

### 2. Added Plugins Tab to Config Dialog

**Files**: 
- `src/application/ui/qt/StructuredConfigDialog.hpp`
- `src/application/ui/qt/StructuredConfigDialog.cpp`

**New Features**:
- **Plugins List**: Table showing all loaded plugins with name, ID, version, type, status, enabled state
- **Plugin Details**: Display selected plugin metadata (author, description, license status, etc.)
- **Load Plugin**: Browse and load new plugin files (.dylib/.so/.dll)
- **Unload Plugin**: Remove loaded plugins
- **Enable/Disable**: Toggle plugin activation
- **License Management**: Set license keys for commercial plugins
- **Auto-load**: Configure plugins to load automatically on startup
- **Refresh**: Discover plugins in plugins directory

**UI Layout**:
```
┌─ Available Plugins ─────────────────────────────────┐
│ Name    │ ID     │ Version │ Type     │ Status │ En │
│ SSH Par │ ssh_p  │ 1.0.0   │ Connect  │ Active │ Y  │
│ CSV Par │ csv_p  │ 1.0.0   │ Parser   │ Init   │ Y  │
└─────────────────────────────────────────────────────┘
┌─ Plugin Details ────────────────────────────────────┐
│ ID: ssh_parser                                       │
│ Name: SSH Parser Plugin                             │
│ Version: 1.0.0                                       │
│ Author: LogViewer Team                               │
│ Type: Connector                                      │
│ Status: Active                                       │
│ License Required: Yes (Licensed)                     │
└─────────────────────────────────────────────────────┘
┌─ Plugin Management ─────────────────────────────────┐
│ Plugin File: [_________________] [Browse] [Load]     │
│ License Key: [*****************] [Set License]       │
│ [✓] Auto-load    [Enable/Disable] [Unload]          │
└─────────────────────────────────────────────────────┘
```

**User Workflow**:
1. Open Settings dialog
2. Navigate to "Plugins" tab
3. Click "Browse..." to select plugin file
4. Click "Load Plugin" to load it
5. Enter license key if required and click "Set License"
6. Enable auto-load if desired
7. Plugin is now available for use

### 3. Example Parser Plugin

**Files**:
- `docs/examples/ExampleParserPlugin.hpp`
- `docs/examples/ExampleParserPlugin.cpp`
- `docs/examples/README.md`

**Demonstrates**:
- Implementing both IPlugin and IDataParser interfaces
- Dual inheritance pattern: `class MyPlugin : public IPlugin, public IDataParser`
- Returning `this` from `GetParserInterface()`
- File extension support
- Observer pattern for progress/events
- License management
- Error handling
- Parsing custom log format (TIMESTAMP|LEVEL|MESSAGE)

**Code Structure**:
```cpp
class ExampleParserPlugin : public plugin::IPlugin, public parser::IDataParser
{
    // IPlugin implementation
    bool Initialize() override;
    void Shutdown() override;
    PluginMetadata GetMetadata() const override;
    parser::IDataParser* GetParserInterface() override { return this; }
    bool SupportsExtension(const std::string& ext) const override;
    
    // IDataParser implementation  
    void Parse(const std::filesystem::path& path) override;
    void ParseStream(std::istream& stream) override;
    void RegisterObserver(IDataParserObserver* obs) override;
    // ... other IDataParser methods
};
```

### 4. Updated Documentation

**File**: `docs/PLUGIN_SYSTEM.md`

**Added Sections**:
- Type-Specific Interfaces explanation
- Parser plugin requirements (IPlugin + IDataParser)
- Filter plugin requirements (IPlugin + IFilterStrategy)
- Code examples for each type
- Interface access patterns

**Key Documentation Points**:
- Parser plugins **MUST** implement IDataParser
- Filter plugins **MUST** implement IFilterStrategy
- Use `GetParserInterface()` to access type-specific methods
- File extension detection for automatic parser selection
- Complete example implementation reference

## Integration with Existing Systems

### ParserFactory Integration

Parser plugins can be registered with ParserFactory:

```cpp
auto& pluginMgr = plugin::PluginManager::GetInstance();
auto* plugin = pluginMgr.GetPlugin("example_parser");
auto* parser = plugin->GetParserInterface();

if (parser) {
    // Register with factory
    ParserFactory::RegisterParser(".example", [parser]() {
        return parser;
    });
}
```

### Automatic File Extension Handling

```cpp
// Find parser plugin that supports extension
auto& pluginMgr = plugin::PluginManager::GetInstance();
for (const auto& [id, info] : pluginMgr.GetLoadedPlugins()) {
    if (info.instance->SupportsExtension(".myformat")) {
        auto* parser = info.instance->GetParserInterface();
        if (parser) {
            // Use this parser
            parser->Parse(filePath);
            break;
        }
    }
}
```

### Filter Integration

```cpp
auto& pluginMgr = plugin::PluginManager::GetInstance();
auto* plugin = pluginMgr.GetPlugin("my_filter");
auto* filter = plugin->GetFilterInterface();

if (filter) {
    // Use filter with FilterManager
    FilterManager::AddStrategy(filter);
}
```

## Plugin Development Workflow

### 1. Choose Plugin Type

- **Parser**: Log file format parsing → Implement IDataParser
- **Filter**: Log event filtering → Implement IFilterStrategy
- **Connector**: Remote connections (SSH, DB, etc.)
- **Exporter**: Export to different formats
- **Analyzer**: Log analysis tools
- **Visualizer**: Custom visualizations
- **Custom**: Other functionality

### 2. Implement Required Interfaces

**For Parser plugins**:
```cpp
class MyParser : public plugin::IPlugin, public parser::IDataParser {
    // Both interfaces required!
};
```

**For Filter plugins**:
```cpp
class MyFilter : public plugin::IPlugin, public filters::IFilterStrategy {
    // Both interfaces required!
};
```

**For other types**:
```cpp
class MyPlugin : public plugin::IPlugin {
    // Only IPlugin required
    // Define custom methods as needed
};
```

### 3. Implement Type-Specific Access

```cpp
// Parser plugin
parser::IDataParser* GetParserInterface() override { 
    return this;  // Return self as IDataParser
}

// Filter plugin
filters::IFilterStrategy* GetFilterInterface() override { 
    return this;  // Return self as IFilterStrategy
}
```

### 4. Build as Shared Library

```cmake
add_library(my_plugin SHARED
    MyPlugin.cpp
    MyPlugin.hpp
)

set_target_properties(my_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "my_plugin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
```

### 5. Load via Config Dialog

1. Open Settings → Plugins tab
2. Browse to plugin file
3. Click "Load Plugin"
4. Set license if required
5. Enable plugin

## Testing

### Manual Testing

1. **Build Example Plugin**:
   ```bash
   # Copy example files to src/plugins/example_parser/
   mkdir -p src/plugins/example_parser
   cp docs/examples/ExampleParserPlugin.* src/plugins/example_parser/
   
   # Add CMakeLists.txt
   # Build
   cmake -B build -DENABLE_EXAMPLE_PARSER=ON
   cmake --build build
   ```

2. **Test via Config Dialog**:
   - Run LogViewer
   - Open Settings → Plugins tab
   - Load `build/plugins/example_parser_plugin.dylib`
   - Verify plugin appears in list
   - Check plugin details display correctly
   - Test enable/disable functionality

3. **Test Parsing**:
   - Create test file with `.example` extension
   - Open via File → Open
   - Verify plugin is used for parsing
   - Check events appear in UI

### Automated Testing

Add unit tests:
```cpp
TEST(PluginSystem, ParserPluginInterfaces) {
    auto plugin = std::make_unique<ExampleParserPlugin>();
    
    // Test IPlugin interface
    ASSERT_TRUE(plugin->Initialize());
    ASSERT_EQ(plugin->GetStatus(), PluginStatus::Initialized);
    
    // Test parser interface access
    auto* parser = plugin->GetParserInterface();
    ASSERT_NE(parser, nullptr);
    
    // Test extension support
    ASSERT_TRUE(plugin->SupportsExtension(".example"));
    ASSERT_FALSE(plugin->SupportsExtension(".unknown"));
    
    // Test parsing
    std::stringstream ss("2025-12-07 10:00:00|INFO|Test message");
    parser->ParseStream(ss);
    ASSERT_GT(parser->GetParsedEvents(), 0);
}
```

## Migration Guide

### For Existing Plugins

If you have existing plugins, update them:

**Before**:
```cpp
class MyPlugin : public plugin::IPlugin {
    // Only IPlugin
};
```

**After** (for Parser plugins):
```cpp
class MyPlugin : public plugin::IPlugin, public parser::IDataParser {
    // Implement both interfaces
    parser::IDataParser* GetParserInterface() override { return this; }
    bool SupportsExtension(const std::string& ext) const override {
        return ext == ".myformat";
    }
};
```

### For Plugin Consumers

**Before**:
```cpp
auto* plugin = manager.GetPlugin("my_parser");
// Cast to specific type manually
auto* myParser = dynamic_cast<MySpecificParser*>(plugin);
```

**After**:
```cpp
auto* plugin = manager.GetPlugin("my_parser");
auto* parser = plugin->GetParserInterface();
// Use standard IDataParser interface
if (parser) {
    parser->Parse(filePath);
}
```

## Benefits

1. **Type Safety**: Compile-time checking of plugin capabilities
2. **Standardization**: All parser plugins use same IDataParser interface
3. **Discoverability**: UI can list plugins by type
4. **Extension Mapping**: Automatic parser selection based on file extension
5. **Integration**: Easy integration with existing ParserFactory and FilterManager
6. **Flexibility**: Plugins can implement multiple interfaces if needed
7. **User-Friendly**: Config dialog provides GUI for plugin management

## Future Enhancements

### Short Term
- Add filter plugin example
- Implement ParserFactory auto-registration for plugin parsers
- Add plugin search/filter in config dialog
- Save plugin configuration to config.json

### Medium Term
- Plugin dependency resolution UI
- Plugin update notifications
- Plugin marketplace/repository
- Sandboxing for untrusted plugins

### Long Term
- Hot-reloading of plugins
- Plugin API versioning
- Remote plugin installation
- Plugin signing/verification

## Conclusion

The plugin system now supports proper type-specific interfaces, making it easy to create parser and filter plugins that integrate seamlessly with LogViewer's existing architecture. The config dialog provides a user-friendly interface for plugin management, and the example implementation serves as a template for plugin developers.

Key achievements:
- ✅ Type-specific interface support (IDataParser, IFilterStrategy)
- ✅ Plugin registration UI in config dialog
- ✅ Complete example parser plugin
- ✅ Updated documentation
- ✅ Extension-based parser selection
- ✅ License management UI
- ✅ Enable/disable functionality
- ✅ Auto-load configuration
