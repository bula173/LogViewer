#include "FieldConversionPluginRegistry.hpp"
#include "IPlugin.hpp"
#include "Logger.hpp"

#include <algorithm>

namespace config
{

FieldConversionPluginRegistry& FieldConversionPluginRegistry::GetInstance()
{
    static FieldConversionPluginRegistry instance;
    return instance;
}

void FieldConversionPluginRegistry::RegisterPlugin(ConversionPluginFactory factory)
{
    RegisterPlugin(std::move(factory), "");
}

void FieldConversionPluginRegistry::RegisterPlugin(ConversionPluginFactory factory, std::string pluginId)
{
    m_factories.emplace_back(std::move(pluginId), std::move(factory));
    // Clear cached instances so they'll be recreated with new plugin
    m_instances.clear();
}

std::vector<std::string> FieldConversionPluginRegistry::GetAvailableConversions() const
{
    EnsureInstancesLoaded();
    std::vector<std::string> conversions;
    for (const auto& instance : m_instances)
    {
        conversions.push_back(instance->GetConversionType());
    }
    return conversions;
}

std::string FieldConversionPluginRegistry::GetDescription(const std::string& conversionType) const
{
    EnsureInstancesLoaded();
    for (const auto& instance : m_instances)
    {
        if (instance->GetConversionType() == conversionType)
        {
            return instance->GetDescription();
        }
    }
    return "";
}

std::string FieldConversionPluginRegistry::ApplyConversion(const std::string& conversionType,
                                                          std::string_view value,
                                                          const std::map<std::string, std::string>& parameters) const
{
    EnsureInstancesLoaded();
    for (const auto& instance : m_instances)
    {
        if (instance->GetConversionType() == conversionType)
        {
            return instance->Convert(value, parameters);
        }
    }
    util::Logger::Warn("Unknown conversion type: {}", conversionType);
    return std::string(value);
}

void FieldConversionPluginRegistry::EnsureInstancesLoaded() const
{
    if (m_instances.size() == m_factories.size())
        return;

    m_instances.clear();
    for (const auto& entry : m_factories)
    {
        try
        {
            m_instances.push_back(entry.second());
        }
        catch (const std::exception& ex)
        {
            util::Logger::Error("Failed to create field conversion plugin: {}", ex.what());
        }
    }
}

void FieldConversionPluginRegistry::OnPluginEvent(plugin::PluginEvent event, 
                                                  const std::string& pluginId,
                                                  plugin::IPlugin* plugin)
{
    using plugin::PluginEvent;
    using plugin::PluginType;
    
    switch (event)
    {
        case PluginEvent::Enabled:
            // Register field conversion plugins when enabled
            if (plugin && plugin->GetMetadata().type == PluginType::FieldConversion)
            {
                RegisterFieldConversionPlugin(plugin);
            }
            break;
            
        case PluginEvent::Disabled:
        case PluginEvent::Unloaded:
            // Unregister when disabled or unloaded
            UnregisterFieldConversionPlugin(pluginId);
            break;
            
        case PluginEvent::Loaded:
        case PluginEvent::Registered:
        case PluginEvent::Unregistered:
            // No action needed for these events
            break;
    }
}

void FieldConversionPluginRegistry::RegisterFieldConversionPlugin(plugin::IPlugin* plugin)
{
    if (!plugin)
        return;
        
    auto* converter = plugin->GetFieldConversionInterface();
    if (!converter)
    {
        util::Logger::Warn("FieldConversionPluginRegistry: Plugin {} does not provide field conversion interface",
            plugin->GetMetadata().id);
        return;
    }
    
    const auto& metadata = plugin->GetMetadata();
    
    // Store the converter pointer for direct access
    m_pluginConverters[metadata.id] = converter;
    
    // Create a factory that returns a non-owning wrapper
    auto factory = [converter]() -> std::unique_ptr<plugin::IFieldConversionPlugin> {
        class FieldConversionWrapper : public plugin::IFieldConversionPlugin {
            plugin::IFieldConversionPlugin* m_converter;
        public:
            explicit FieldConversionWrapper(plugin::IFieldConversionPlugin* converter) 
                : m_converter(converter) {}
            
            std::string GetConversionType() const override { 
                return m_converter->GetConversionType(); 
            }
            std::string GetDescription() const override { 
                return m_converter->GetDescription(); 
            }
            std::string Convert(std::string_view value, 
                              const std::map<std::string, std::string>& parameters) const override {
                return m_converter->Convert(value, parameters);
            }
            bool CanConvert(std::string_view value) const override {
                return m_converter->CanConvert(value);
            }
            std::string GetConversionName() const override {
                return m_converter->GetConversionName();
            }
        };
        return std::make_unique<FieldConversionWrapper>(converter);
    };
    
    RegisterPlugin(factory, metadata.id);
    util::Logger::Info("FieldConversionPluginRegistry: Registered field conversion plugin: {} ({})",
        metadata.name, converter->GetConversionType());
}

void FieldConversionPluginRegistry::UnregisterFieldConversionPlugin(const std::string& pluginId)
{
    auto it = m_pluginConverters.find(pluginId);
    if (it == m_pluginConverters.end())
        return;
        
    // Remove from tracking map
    m_pluginConverters.erase(it);

    // Remove factories tied to this pluginId
    m_factories.erase(
        std::remove_if(m_factories.begin(), m_factories.end(), [&](const auto& entry) {
            return entry.first == pluginId;
        }),
        m_factories.end());

    // Force reload of instances on next access
    m_instances.clear();
    
    util::Logger::Info("FieldConversionPluginRegistry: Unregistered field conversion plugin: {}", pluginId);
}

} // namespace config
