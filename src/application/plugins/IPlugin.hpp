/**
 * @file IPlugin.hpp
 * @brief Generic plugin interface for LogViewer extensions
 * @author LogViewer Development Team
 * @date 2025
 */

#ifndef PLUGIN_IPLUGIN_HPP
#define PLUGIN_IPLUGIN_HPP

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

/**
 * @enum PluginType
 * @brief Types of plugins supported by LogViewer
 */
enum class PluginType
{
    Parser,        ///< Data parser plugin (CSV, SSH, etc.)
    Filter,        ///< Custom filter implementation
    FieldConversion, ///< Field conversion/transformation plugin
    Exporter,      ///< Data export functionality
    Analyzer,      ///< Log analysis tool
    AIProvider,    ///< Provides AI service and UI
    Connector,     ///< Remote connection (SSH, network, etc.)
    Visualizer,    ///< Custom visualization component
    Custom         ///< Generic custom plugin
};

/**
 * @enum PluginStatus
 * @brief Plugin lifecycle status
 */
enum class PluginStatus
{
    Unloaded,      ///< Plugin not loaded
    Loaded,        ///< Plugin loaded successfully
    Initialized,   ///< Plugin initialized and ready
    Active,        ///< Plugin actively running
    Error,         ///< Plugin in error state
    Disabled       ///< Plugin disabled by user
};

/**
 * @struct PluginMetadata
 * @brief Plugin metadata and information
 */
struct PluginMetadata
{
    std::string id;              ///< Unique plugin identifier
    std::string name;            ///< Display name
    std::string version;         ///< Plugin version (e.g., "1.0.0")
    std::string apiVersion {"1.0.0"}; ///< Plugin API compatibility version
    std::string author;          ///< Plugin author
    std::string description;     ///< Brief description
    std::string website;         ///< Plugin website/documentation
    PluginType type;             ///< Plugin type
    bool requiresLicense = false; ///< Whether plugin needs license
    std::vector<std::string> dependencies; ///< Required dependencies
};

/**
 * @class IPlugin
 * @brief Base interface for all LogViewer plugins
 * 
 * This interface defines the contract that all plugins must implement.
 * Plugins can extend LogViewer functionality in various ways:
 * - Custom data parsers
 * - Remote connectors (SSH, network protocols)
 * - Analysis tools
 * - Export formats
 * - Visualization components
 */
class IPlugin
{
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IPlugin() = default;

    /**
     * @brief Gets plugin metadata
     * @return Plugin metadata structure
     */
    virtual PluginMetadata GetMetadata() const = 0;

    /**
     * @brief Initializes the plugin
     * @return True if initialization successful
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Shuts down the plugin
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Gets current plugin status
     * @return Current status
     */
    virtual PluginStatus GetStatus() const = 0;

    /**
     * @brief Gets last error message
     * @return Error description
     */
    virtual std::string GetLastError() const = 0;

    /**
     * @brief Checks if plugin is licensed
     * @return True if licensed or license not required
     */
    virtual bool IsLicensed() const = 0;

    /**
     * @brief Sets license key for the plugin
     * @param licenseKey License key string
     * @return True if license valid
     */
    virtual bool SetLicense(const std::string& licenseKey) = 0;

    /**
     * @brief Gets type-specific parser interface (for Parser plugins)
     * @return Pointer to IDataParser interface or nullptr if not a parser
     * @note Only valid when GetMetadata().type == PluginType::Parser
     */
    virtual parser::IDataParser* GetParserInterface() { return nullptr; }

    /**
     * @brief Gets type-specific filter interface (for Filter plugins)
     * @return Pointer to IFilterStrategy interface or nullptr if not a filter
     * @note Only valid when GetMetadata().type == PluginType::Filter
     */
    virtual filters::IFilterStrategy* GetFilterInterface() { return nullptr; }

    /**
     * @brief Gets type-specific analysis interface (for Analyzer plugins)
     * @return Pointer to IAnalysisPlugin interface or nullptr if not an analyzer
     * @note Only valid when GetMetadata().type == PluginType::Analyzer
     */
    virtual IAnalysisPlugin* GetAnalysisInterface() { return nullptr; }

    /**
     * @brief Gets AI provider interface (for AIProvider plugins)
     * @return Pointer to IAIPlugin interface or nullptr if not an AI provider
     * @note Only valid when GetMetadata().type == PluginType::AIProvider
     */
    virtual IAIPlugin* GetAIPluginInterface() { return nullptr; }

    /**
     * @brief Gets type-specific field conversion interface (for FieldConversion plugins)
     * @return Pointer to IFieldConversionPlugin interface or nullptr if not a field converter
     * @note Only valid when GetMetadata().type == PluginType::FieldConversion
     */
    virtual IFieldConversionPlugin* GetFieldConversionInterface() { return nullptr; }

    /**
     * @brief Checks if plugin supports given file extension (for Parser plugins)
     * @param extension File extension with dot (e.g., ".log", ".csv")
     * @return True if plugin can parse files with this extension
     */
    virtual bool SupportsExtension([[maybe_unused]] const std::string& extension) const { return false; }

    /**
     * @brief Gets list of supported file extensions (for Parser plugins)
     * @return Vector of supported extensions (e.g., {".log", ".txt"})
     */
    virtual std::vector<std::string> GetSupportedExtensions() const { return {}; }

    /**
     * @brief Gets plugin configuration UI (optional)
     * @return Pointer to configuration widget/dialog, nullptr if not supported
     */
    virtual QWidget* GetConfigurationUI() { return nullptr; }

    /**
     * @brief Validates plugin configuration
     * @return True if configuration is valid
     */
    virtual bool ValidateConfiguration() const = 0;

    /**
     * @brief Creates a new tab for the plugin
     * @param parent Parent widget for the tab
     * @return Pointer to the created tab widget, or nullptr on failure
     * @note This method is optional and may not be supported by all plugins
     */
    virtual QWidget* CreateTab([[maybe_unused]] QWidget* parent) { return nullptr; }

    /**
     * @brief Creates a new filter tab for the plugin
     * @param parent Parent widget for the filter tab
     * @return Pointer to the created filter tab widget, or nullptr if not supported
     * @note This allows plugins to add custom filter/configuration panels
     */
    virtual QWidget* CreateFilterTab([[maybe_unused]]QWidget* parent) { return nullptr; }

    /**
     * @brief Creates a bottom dock widget (e.g., AI chat) for this plugin
     * @param parent Parent widget for ownership
     * @param service Optional AI service for AIProvider plugins
     * @param events Optional events container for context
     * @return QWidget to embed in the bottom dock, or nullptr if not provided
     */
    virtual QWidget* CreateBottomPanel(
        [[maybe_unused]] QWidget* parent,
        [[maybe_unused]] ai::IAIService* service,
        [[maybe_unused]] db::EventsContainer* events) { return nullptr; }
};

/**
 * @brief Plugin factory function type
 */
using PluginFactoryFunc = std::unique_ptr<IPlugin>(*)();

/**
 * @brief Macro to export plugin factory function
 * 
 * Usage in plugin implementation:
 * EXPORT_PLUGIN(MyPluginClass)
 */
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

#endif // PLUGIN_IPLUGIN_HPP
