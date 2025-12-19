#pragma once

#include "IFieldConversionPlugin.hpp"
#include "IPluginObserver.hpp"
#include <string>
#include <string_view>
#include <map>
#include <memory>
#include <functional>
#include <vector>

namespace config
{

/**
 * @brief Factory function type for creating conversion plugins
 */
using ConversionPluginFactory = std::function<std::unique_ptr<plugin::IFieldConversionPlugin>()>;

/**
 * @brief Registry for field conversion plugins
 * 
 * This registry maintains a collection of field conversion plugins that can be
 * used throughout the application for transforming field values.
 * Implements IPluginObserver to automatically register/unregister plugins
 * based on their lifecycle events.
 */
class FieldConversionPluginRegistry : public plugin::IPluginObserver
{
public:
    static FieldConversionPluginRegistry& GetInstance();

    /**
     * @brief Register a conversion plugin
     * @param factory Function that creates the plugin instance
     */
    void RegisterPlugin(ConversionPluginFactory factory);
    void RegisterPlugin(ConversionPluginFactory factory, std::string pluginId);

    /**
     * @brief Get all registered conversion types
     * @return Vector of conversion type names
     */
    std::vector<std::string> GetAvailableConversions() const;

    /**
     * @brief Get description for a conversion type
     * @param conversionType The conversion type name
     * @return Description string, or empty if not found
     */
    std::string GetDescription(const std::string& conversionType) const;

    /**
     * @brief Apply a conversion using registered plugins
     * @param conversionType The conversion type to use
     * @param value The value to convert
     * @param parameters Additional parameters
     * @return Converted value, or original value if conversion failed/not found
     */
    std::string ApplyConversion(const std::string& conversionType,
                               std::string_view value,
                               const std::map<std::string, std::string>& parameters) const;

    // IPluginObserver interface
    void OnPluginEvent(plugin::PluginEvent event, 
                      const std::string& pluginId,
                      plugin::IPlugin* plugin) override;

private:
    void EnsureInstancesLoaded() const;
    void RegisterFieldConversionPlugin(plugin::IPlugin* plugin);
    void UnregisterFieldConversionPlugin(const std::string& pluginId);

    FieldConversionPluginRegistry() = default;
    std::vector<std::pair<std::string, ConversionPluginFactory>> m_factories;
    mutable std::vector<std::unique_ptr<plugin::IFieldConversionPlugin>> m_instances;
    
    // Track which plugins are registered by plugin ID
    std::map<std::string, plugin::IFieldConversionPlugin*> m_pluginConverters;
};

} // namespace config
