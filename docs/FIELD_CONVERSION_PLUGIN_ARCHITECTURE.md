# Field Conversion Plugin Architecture

## Overview

Field conversion plugins have been refactored to follow the same pattern as analysis plugins, integrating them into the main plugin system with the Observer pattern.

## Architecture

### IFieldConversionPlugin Interface

Located in `src/application/plugins/IFieldConversionPlugin.hpp`, this interface defines the contract for field conversion plugins:

```cpp
class IFieldConversionPlugin
{
public:
    virtual ~IFieldConversionPlugin() = default;
    
    // Plugin identification
    virtual std::string GetConversionType() const = 0;
    virtual std::string GetConversionName() const = 0;
    virtual std::string GetDescription() const = 0;
    
    // Conversion logic
    virtual std::string Convert(std::string_view value, 
                                const std::map<std::string, std::string>& parameters) const = 0;
    
    // Optional validation
    virtual bool CanConvert(std::string_view value) const { return true; }
};
```

### Integration with IPlugin

Field conversion plugins implement both `IPlugin` and `IFieldConversionPlugin`:

```cpp
class MyConverterPlugin : public plugin::IPlugin, 
                          public plugin::IFieldConversionPlugin
{
public:
    // IPlugin implementation
    PluginMetadata GetMetadata() const override {
        return PluginMetadata{
            "my_converter",              // id
            "My Converter",             // name
            "1.0.0",                   // version
            "Author Name",             // author
            "Converts fields",         // description
            "",                        // website
            PluginType::FieldConversion, // type
            false,                     // requiresLicense
            {}                         // dependencies
        };
    }
    
    bool Initialize() override { return true; }
    void Shutdown() override {}
    PluginStatus GetStatus() const override { return m_status; }
    std::string GetLastError() const override { return ""; }
    bool IsLicensed() const override { return true; }
    bool SetLicense(const std::string&) override { return true; }
    bool ValidateConfiguration() const override { return true; }
    
    // Return this plugin as the field conversion interface
    IFieldConversionPlugin* GetFieldConversionInterface() override {
        return this;
    }
    
    // IFieldConversionPlugin implementation
    std::string GetConversionType() const override {
        return "my_conversion";
    }
    
    std::string GetConversionName() const override {
        return "My Conversion";
    }
    
    std::string GetDescription() const override {
        return "Detailed description of conversion";
    }
    
    std::string Convert(std::string_view value, 
                       const std::map<std::string, std::string>& parameters) const override {
        // Conversion logic here
        return std::string(value);
    }
};
```

## Registration Flow

### 1. Plugin Load

When PluginManager loads a field conversion plugin:

```cpp
// LoadPlugin() in PluginManager.cpp
if (metadata.type == PluginType::FieldConversion) {
    auto* fieldConverter = plugin->GetFieldConversionInterface();
    if (fieldConverter) {
        // Create a factory wrapper
        auto factory = [fieldConverter]() -> std::unique_ptr<plugin::IFieldConversionPlugin> {
            return std::make_unique<FieldConversionWrapper>(fieldConverter);
        };
        
        // Register with legacy registry
        config::FieldConversionPluginRegistry::GetInstance().RegisterPlugin(factory);
    }
}
```

### 2. Observer Notification

The plugin system notifies observers about the field conversion plugin:

```cpp
// PluginEvent::Loaded → observers notified
// PluginEvent::Enabled → available for use
// PluginEvent::Disabled → temporarily unavailable
// PluginEvent::Unloaded → removed from system
```

## Backward Compatibility

The old `config::IFieldConversionPlugin` namespace is maintained for backward compatibility:

```cpp
// In config/IFieldConversionPlugin.hpp
namespace config {
    // Re-export plugin interface
    using IFieldConversionPlugin = plugin::IFieldConversionPlugin;
    using ConversionPluginFactory = std::function<std::unique_ptr<plugin::IFieldConversionPlugin>()>;
}
```

This allows existing code to continue working without modifications.

## Built-in Converters

Built-in conversion plugins are located in `src/application/config/`:

- **HexToAsciiPlugin**: Converts hexadecimal strings to ASCII
- **UnixToDatePlugin**: Converts Unix timestamps to readable dates
- **ValueMapPlugin**: Maps values using lookup tables
- **IsoLatinPlugin**: Converts ISO Latin-1 to UTF-8
- **NidLrbgPlugin**: Converts railway identifiers

All builtin converters implement the new interface with `GetConversionName()`.

## Example: Creating a Field Conversion Plugin

### 1. Create Plugin Class

```cpp
// ToBase64Plugin.cpp
#include "application/plugins/IPlugin.hpp"
#include "application/plugins/IFieldConversionPlugin.hpp"
#include <sstream>

namespace plugin {

class ToBase64Plugin : public IPlugin, public IFieldConversionPlugin
{
public:
    ToBase64Plugin() : m_status(PluginStatus::Unloaded) {}
    
    // IPlugin interface
    PluginMetadata GetMetadata() const override {
        return PluginMetadata{
            "to_base64",
            "Base64 Encoder",
            "1.0.0",
            "Your Name",
            "Encodes strings to Base64",
            "",
            PluginType::FieldConversion,
            false,
            {}
        };
    }
    
    bool Initialize() override {
        m_status = PluginStatus::Initialized;
        return true;
    }
    
    void Shutdown() override {
        m_status = PluginStatus::Unloaded;
    }
    
    PluginStatus GetStatus() const override { return m_status; }
    std::string GetLastError() const override { return m_lastError; }
    bool IsLicensed() const override { return true; }
    bool SetLicense(const std::string&) override { return true; }
    bool ValidateConfiguration() const override { return true; }
    
    IFieldConversionPlugin* GetFieldConversionInterface() override {
        return this;
    }
    
    // IFieldConversionPlugin interface
    std::string GetConversionType() const override {
        return "to_base64";
    }
    
    std::string GetConversionName() const override {
        return "Base64 Encode";
    }
    
    std::string GetDescription() const override {
        return "Encodes text to Base64 format";
    }
    
    std::string Convert(std::string_view value, 
                       const std::map<std::string, std::string>&) const override {
        // Base64 encoding logic
        static const char* base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        
        std::string result;
        // ... encoding implementation ...
        return result;
    }
    
    bool CanConvert(std::string_view value) const override {
        // Validate that value can be encoded
        return !value.empty();
    }

private:
    PluginStatus m_status;
    std::string m_lastError;
};

} // namespace plugin

// Export plugin
EXPORT_PLUGIN(plugin::ToBase64Plugin)
```

### 2. Build Configuration

```cmake
# CMakeLists.txt
add_library(to_base64 SHARED ToBase64Plugin.cpp)
target_link_libraries(to_base64 PRIVATE application_core)
set_target_properties(to_base64 PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

# Copy to dist/plugins
add_custom_command(TARGET to_base64 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:to_base64>
        ${DIST_PLUGINS_DIR}/
)
```

### 3. Usage in Configuration

In `config.json`:

```json
{
    "dictionary": [
        {
            "key": "encoded_data",
            "conversion": "to_base64"
        }
    ]
}
```

## Benefits of New Architecture

1. **Unified Plugin System**: Field converters are now first-class plugins
2. **Observer Pattern**: Consistent event notification across all plugin types
3. **Lifecycle Management**: Enable/Disable without reloading
4. **Better Discoverability**: Field converters appear in plugin manager UI
5. **Type Safety**: Stronger type checking with dedicated interface
6. **Extensibility**: Easy to add new conversion methods

## Migration Guide

### Old Approach (Builtin Only)

```cpp
// Old: Direct implementation in config namespace
class MyConverter : public config::IFieldConversionPlugin {
    std::string GetConversionType() const override { return "my_type"; }
    std::string GetDescription() const override { return "Description"; }
    std::string Convert(std::string_view value, ...) const override { ... }
};

// Register directly
config::FieldConversionPluginRegistry::GetInstance()
    .RegisterPlugin([]() { return std::make_unique<MyConverter>(); });
```

### New Approach (Full Plugin)

```cpp
// New: Full plugin implementation
class MyConverter : public plugin::IPlugin, 
                    public plugin::IFieldConversionPlugin {
    // IPlugin methods...
    PluginMetadata GetMetadata() const override { ... }
    bool Initialize() override { ... }
    IFieldConversionPlugin* GetFieldConversionInterface() override { return this; }
    
    // IFieldConversionPlugin methods...
    std::string GetConversionType() const override { return "my_type"; }
    std::string GetConversionName() const override { return "My Converter"; }
    std::string GetDescription() const override { return "Description"; }
    std::string Convert(std::string_view value, ...) const override { ... }
};

// Export as plugin - auto-registered by PluginManager
EXPORT_PLUGIN(plugin::MyConverter)
```

## Best Practices

1. **Thread Safety**: Conversion methods should be thread-safe and const
2. **Error Handling**: Return original value on conversion failure
3. **Validation**: Implement `CanConvert()` for pre-validation
4. **Performance**: Keep conversions fast (avoid I/O, heavy computation)
5. **Parameters**: Use parameters map for configuration (e.g., encoding, format)
6. **Naming**: Use clear, descriptive names for `GetConversionName()`

## Testing

```cpp
// Test your field conversion plugin
TEST(MyConverterTest, BasicConversion) {
    plugin::MyConverter converter;
    
    // Test conversion
    std::string result = converter.Convert("input", {});
    EXPECT_EQ(result, "expected_output");
    
    // Test validation
    EXPECT_TRUE(converter.CanConvert("valid_input"));
    EXPECT_FALSE(converter.CanConvert("invalid_input"));
}
```

## Observer Pattern Integration

Field conversion plugins participate in the observer pattern:

```cpp
// Observer receives events for field conversion plugins
void OnPluginEvent(PluginEvent event, const std::string& pluginId, IPlugin* plugin) {
    if (event == PluginEvent::Enabled && 
        plugin->GetMetadata().type == PluginType::FieldConversion) {
        
        auto* converter = plugin->GetFieldConversionInterface();
        if (converter) {
            // Update UI with new converter
            addConverterToUI(converter->GetConversionName());
        }
    }
}
```

This allows UI components to dynamically update when field conversion plugins are loaded or unloaded.
