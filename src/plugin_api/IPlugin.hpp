// Lightweight SDK copy of plugin types for third-party plugins
#pragma once

#include <string>
#include <vector>

namespace plugin
{
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

} // namespace plugin
