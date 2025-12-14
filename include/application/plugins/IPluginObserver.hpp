/**
 * @file IPluginObserver.hpp
 * @brief Observer interface for plugin lifecycle events
 * @author LogViewer Development Team
 * @date 2025
 */

#ifndef IPLUGIN_OBSERVER_HPP
#define IPLUGIN_OBSERVER_HPP

#include <string>

namespace plugin
{

// Forward declarations
class IPlugin;

/**
 * @enum PluginEvent
 * @brief Types of plugin lifecycle events
 */
enum class PluginEvent
{
    Loaded,      ///< Plugin was loaded into memory
    Unloaded,    ///< Plugin was unloaded from memory
    Enabled,     ///< Plugin was enabled (activated)
    Disabled,    ///< Plugin was disabled (deactivated)
    Registered,  ///< Plugin was registered (file copied to user dir)
    Unregistered ///< Plugin was unregistered (file removed from user dir)
};

/**
 * @class IPluginObserver
 * @brief Interface for observers of plugin lifecycle events
 * 
 * Implement this interface to receive notifications about plugin
 * state changes. Register observers with PluginManager to receive
 * events.
 */
class IPluginObserver
{
public:
    virtual ~IPluginObserver() = default;

    /**
     * @brief Called when a plugin event occurs
     * @param event Type of event
     * @param pluginId ID of the plugin
     * @param plugin Pointer to plugin instance (nullptr for unload/unregister events)
     */
    virtual void OnPluginEvent(PluginEvent event, 
                              const std::string& pluginId,
                              IPlugin* plugin) = 0;
};

} // namespace plugin

#endif // IPLUGIN_OBSERVER_HPP
