/**
 * @file IPlugin.hpp
 * @brief Generic plugin interface for LogViewer extensions
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

// Forward declarations for type-specific interfaces
namespace parser { class IDataParser; }
namespace filters { class IFilterStrategy; }
namespace ai { class IAIService; }
namespace db { class EventsContainer; }

namespace plugin { 
    class IAnalysisPlugin; 
    class IFieldConversionPlugin;
    class IAIPlugin;
}

// Forward declare QWidget for GUI plugins
class QWidget;

namespace plugin
{
enum class PluginType { Parser, Filter, FieldConversion, Exporter, Analyzer, AIProvider, Connector, Visualizer, Custom };
enum class PluginStatus { Unloaded, Loaded, Initialized, Active, Error, Disabled };

struct PluginMetadata { std::string id; std::string name; std::string version; std::string apiVersion{"1.0.0"}; std::string author; std::string description; std::string website; PluginType type; bool requiresLicense=false; std::vector<std::string> dependencies; };

class IPlugin
{
public:
    virtual ~IPlugin() = default;
    virtual PluginMetadata GetMetadata() const = 0;
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual PluginStatus GetStatus() const = 0;
    virtual std::string GetLastError() const = 0;
    virtual bool IsLicensed() const = 0;
    virtual bool SetLicense(const std::string& licenseKey) = 0;
    virtual parser::IDataParser* GetParserInterface() { return nullptr; }
    virtual filters::IFilterStrategy* GetFilterInterface() { return nullptr; }
    virtual IAnalysisPlugin* GetAnalysisInterface() { return nullptr; }
    virtual IAIPlugin* GetAIPluginInterface() { return nullptr; }
    virtual IFieldConversionPlugin* GetFieldConversionInterface() { return nullptr; }
    virtual bool SupportsExtension([[maybe_unused]] const std::string& extension) const { return false; }
    virtual std::vector<std::string> GetSupportedExtensions() const { return {}; }
    virtual QWidget* GetConfigurationUI() { return nullptr; }
    virtual bool ValidateConfiguration() const = 0;
    virtual QWidget* CreateTab([[maybe_unused]] QWidget* parent) { return nullptr; }
    virtual QWidget* CreateFilterTab([[maybe_unused]]QWidget* parent) { return nullptr; }
    virtual QWidget* CreateBottomPanel([[maybe_unused]] QWidget* parent, [[maybe_unused]] ai::IAIService* service, [[maybe_unused]] db::EventsContainer* events) { return nullptr; }
};

using PluginFactoryFunc = std::unique_ptr<IPlugin>(*)();

#ifdef _WIN32
    #define EXPORT_PLUGIN_SYMBOL __declspec(dllexport)
#else
    #define EXPORT_PLUGIN_SYMBOL __attribute__((visibility("default")))
#endif

#define EXPORT_PLUGIN(PluginClass) \
    extern "C" EXPORT_PLUGIN_SYMBOL std::unique_ptr<plugin::IPlugin> CreatePlugin() { \
        return std::make_unique<PluginClass>(); \
    }

} // namespace plugin
