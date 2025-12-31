// IPluginObserver internal interface
#pragma once

#include <string>

namespace plugin {

class IPlugin;

enum class PluginEvent { Loaded, Unloaded, Enabled, Disabled, Registered, Unregistered };

class IPluginObserver
{
public:
    virtual ~IPluginObserver() = default;
    virtual void OnPluginEvent(PluginEvent event, const std::string& pluginId, IPlugin* plugin) = 0;
};

} // namespace plugin
