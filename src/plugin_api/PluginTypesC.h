// C ABI friendly plugin types for SDK consumers
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PluginTypeC {
    PluginTypeC_Parser,
    PluginTypeC_Filter,
    PluginTypeC_FieldConversion,
    PluginTypeC_Exporter,
    PluginTypeC_Analyzer,
    PluginTypeC_AIProvider,
    PluginTypeC_Connector,
    PluginTypeC_Visualizer,
    PluginTypeC_Custom
} PluginTypeC;

typedef enum PluginStatusC { PluginStatusC_Unloaded, PluginStatusC_Loaded, PluginStatusC_Initialized, PluginStatusC_Active, PluginStatusC_Error, PluginStatusC_Disabled } PluginStatusC;

// Plugin metadata represented in C ABI-friendly struct. Strings are NUL-terminated
// and ownership follows caller convention (use PluginApi_FreeString where applicable).
typedef struct PluginMetadataC {
    const char* id;
    const char* name;
    const char* version;
    const char* apiVersion;
    const char* author;
    const char* description;
    const char* website;
    PluginTypeC type;
    int requiresLicense; // bool as int for C compatibility
} PluginMetadataC;

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include <string>
#include <vector>

// C++ friendly enum/classes that mirror the C ABI types so C++ plugins
// can use familiar names without including application internals.
enum class PluginType { Parser, Filter, FieldConversion, Exporter, Analyzer, AIProvider, Connector, Visualizer, Custom };

enum class PluginStatus { Unloaded, Loaded, Initialized, Active, Error, Disabled };

struct PluginMetadata {
    std::string id;
    std::string name;
    std::string version;
    std::string apiVersion{"1.0.0"};
    std::string author;
    std::string description;
    std::string website;
    PluginType type{PluginType::Custom};
    bool requiresLicense{false};
    std::vector<std::string> dependencies;
};

// Conversion helpers could be added if needed to map between PluginMetadata and PluginMetadataC
#endif
