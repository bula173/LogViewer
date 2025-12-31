/**
 * @file PluginManager.hpp
 * @brief Plugin management and registration system
 * @author LogViewer Development Team
 * @date 2025
 */

#ifndef PLUGIN_MANAGER_HPP
#define PLUGIN_MANAGER_HPP

#include "IPlugin.hpp"
#include "IPluginObserver.hpp"
#include "Result.hpp"
#include "Error.hpp"
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace plugin
{

/**
 * @struct PluginLoadInfo
 * @brief Information about a loaded plugin
 */
struct PluginLoadInfo
{
    std::string pluginId;              ///< Plugin ID
    std::filesystem::path path;        ///< Plugin file path
    std::unique_ptr<IPlugin> instance; ///< Plugin instance
    void* libraryHandle = nullptr;     ///< Dynamic library handle
    bool autoLoad = false;             ///< Auto-load on startup
    bool enabled = true;               ///< Plugin enabled
    // Optional C-style AI provider function pointers discovered in plugin DLL
    void* pluginCreateAI = nullptr;
    void* pluginDestroyAI = nullptr;
    void* pluginSetAIEvents = nullptr;
    void* pluginOnAIEventsUpdated = nullptr;
    // Optional C-style Field Conversion provider function pointers
    void* pluginCreateFieldConversion = nullptr;
    void* pluginDestroyFieldConversion = nullptr;
    void* pluginSetFieldConversionEvents = nullptr;
    void* pluginOnFieldConversionUpdated = nullptr;
    // Optional C-style Analysis provider function pointers
    void* pluginCreateAnalysis = nullptr;
    void* pluginDestroyAnalysis = nullptr;
    void* pluginSetAnalysisEvents = nullptr;
    void* pluginOnAnalysisUpdated = nullptr;
};

/**
 * @class PluginManager
 * @brief Manages plugin lifecycle and registration
 * 
 * The PluginManager handles:
 * - Plugin discovery and loading
 * - Plugin registration and unregistration
 * - Plugin configuration persistence
 * - License validation
 * - Dependency resolution
 */
class PluginManager
{
public:
    /**
     * @brief Gets singleton instance
     * @return PluginManager reference
     */
    static PluginManager& GetInstance();

    // Disable copy/move
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /**
     * @brief Sets the plugins directory
     * @param directory Path to plugins directory
     */
    void SetPluginsDirectory(const std::filesystem::path& directory);

    /**
     * @brief Gets the plugins directory
     * @return Path to plugins directory
     */
    std::filesystem::path GetPluginsDirectory() const;

    /**
     * @brief Discovers plugins in the plugins directory
     * @return List of discovered plugin paths
     */
    std::vector<std::filesystem::path> DiscoverPlugins();

    /**
     * @brief Loads a plugin from file
     * @param pluginPath Path to plugin library
     * @return Result with plugin ID or error
     */
    util::Result<std::string, error::Error> LoadPlugin(
        const std::filesystem::path& pluginPath);

    /**
     * @brief Unloads a plugin
     * @param pluginId Plugin identifier
     * @return Result with success or error
     */
    util::Result<bool, error::Error> UnloadPlugin(const std::string& pluginId);

    /**
     * @brief Registers a plugin file (copies to user plugins directory)
     * @param sourcePath Path to plugin file to register
     * @return Result with plugin ID or error
     */
    util::Result<std::string, error::Error> RegisterPlugin(
        const std::filesystem::path& sourcePath);

    /**
     * @brief Unregisters a plugin (removes from user plugins directory)
     * @param pluginId Plugin identifier
     * @return Result with success or error
     */
    util::Result<bool, error::Error> UnregisterPlugin(const std::string& pluginId);

    /**
     * @brief Gets a plugin by ID
     * @param pluginId Plugin identifier
     * @return Pointer to plugin or nullptr
     */
    IPlugin* GetPlugin(const std::string& pluginId);

    /**
     * @brief Gets all loaded plugins
     * @return Map of plugin ID to plugin info
     */
    const std::map<std::string, PluginLoadInfo>& GetLoadedPlugins() const;

    /**
     * @brief Gets plugins by type
     * @param type Plugin type filter
     * @return Vector of plugin IDs matching type
     */
    std::vector<std::string> GetPluginsByType(PluginType type) const;

    /**
     * @brief Enables a plugin (loads and initializes it)
     * @param pluginId Plugin identifier
     * @return Result with success or error
     */
    util::Result<bool, error::Error> EnablePlugin(const std::string& pluginId);

    /**
     * @brief Disables a plugin (unloads it but keeps file registered)
     * @param pluginId Plugin identifier
     * @return Result with success or error
     */
    util::Result<bool, error::Error> DisablePlugin(const std::string& pluginId);

    /**
     * @brief Sets plugin auto-load preference
     * @param pluginId Plugin identifier
     * @param autoLoad Auto-load on startup
     */
    void SetPluginAutoLoad(const std::string& pluginId, bool autoLoad);

    /**
     * @brief Registers an observer for plugin lifecycle events
     * @param observer Observer to register
     */
    void RegisterObserver(IPluginObserver* observer);

    /**
     * @brief Unregisters an observer
     * @param observer Observer to unregister
     */
    void UnregisterObserver(IPluginObserver* observer);

    /**
     * @brief Loads plugin configuration
     * @return Result with success or error
     */
    util::Result<bool, error::Error> LoadConfiguration();

    /**
     * @brief Saves plugin configuration
     * @return Result with success or error
     */
    util::Result<bool, error::Error> SaveConfiguration();

    /**
     * @brief Validates all plugin dependencies
     * @return Result with missing dependencies or success
     */
    util::Result<bool, error::Error> ValidateDependencies() const;

    /**
     * @brief Gets the configuration directory path for a specific plugin
     * @param pluginId Plugin identifier
     * @return Path to plugin's config directory (e.g., ~/Library/Application Support/LogViewer/plugins/<plugin_id>/)
     */
    std::filesystem::path GetPluginConfigDirectory(const std::string& pluginId) const;

    /**
     * @brief Gets the configuration file path for a specific plugin
     * @param pluginId Plugin identifier
     * @return Path to plugin's config file (e.g., ~/Library/Application Support/LogViewer/plugins/<plugin_id>/config.json)
     */
    std::filesystem::path GetPluginConfigPath(const std::string& pluginId) const;

private:
    PluginManager() = default;
    ~PluginManager();

    /**
     * @brief Loads plugin from dynamic library
     * @param path Plugin library path
     * @return Result with plugin instance or error
     */
    util::Result<std::unique_ptr<IPlugin>, error::Error> LoadPluginLibrary(
        const std::filesystem::path& path,
        const std::string& expectedApiVersion,
        /*out*/ void** outLibraryHandle);

    /**
     * @brief Unloads dynamic library
     * @param handle Library handle
     */
    void UnloadLibrary(void* handle);

    /**
     * @brief Notifies all observers of a plugin event
     * @param event Type of event
     * @param pluginId ID of the plugin
     * @param plugin Pointer to plugin instance (nullptr for unload/unregister)
     */
    void NotifyObservers(PluginEvent event, const std::string& pluginId, IPlugin* plugin);

    std::filesystem::path m_pluginsDirectory;
    std::map<std::string, PluginLoadInfo> m_plugins;
    std::map<std::string, std::filesystem::path> m_registeredPlugins;  // pluginId -> file path
    std::filesystem::path m_configPath;
    std::vector<IPluginObserver*> m_observers;
    // Opaque events container pointer provided by application (for C API)
    void* m_eventsContainerOpaque = nullptr;
    
    // Cache for plugin configuration loaded before plugins are instantiated
    struct PluginConfigCache {
        bool enabled;
        bool autoLoad;
    };
    std::map<std::string, PluginConfigCache> m_configCache;

    std::vector<void*> m_dependencyHandles;  // Store handles to loaded dependencies
    // Setter for application to provide opaque events container handle
public:
    void SetEventsContainerOpaque(void* handle) { m_eventsContainerOpaque = handle; }
};

} // namespace plugin

#endif // PLUGIN_MANAGER_HPP
