/**
 * @file PluginManager.cpp
 * @brief Implementation of plugin management system
 * @author LogViewer Development Team
 * @date 2025
 */

#include "PluginManager.hpp"
#include "config/Config.hpp"
#include "config/FieldConversionPluginRegistry.hpp"
#include "util/Logger.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace plugin
{

PluginManager& PluginManager::GetInstance()
{
    static PluginManager instance;
    
    // Register field conversion registry as observer (done once)
    static bool registryObserverRegistered = false;
    if (!registryObserverRegistered) {
        instance.RegisterObserver(&config::FieldConversionPluginRegistry::GetInstance());
        registryObserverRegistered = true;
        util::Logger::Debug("PluginManager: Registered FieldConversionPluginRegistry as observer");
    }
    
    return instance;
}

PluginManager::~PluginManager()
{
    // Unload all plugins
    for (auto& [id, info] : m_plugins)
    {
        if (info.instance)
        {
            info.instance->Shutdown();
        }
        if (info.libraryHandle)
        {
            UnloadLibrary(info.libraryHandle);
        }
    }
}

void PluginManager::SetPluginsDirectory(const std::filesystem::path& directory)
{
    m_pluginsDirectory = directory;
    util::Logger::Info("PluginManager: Plugins directory set to: {}", 
        directory.string());
}

std::filesystem::path PluginManager::GetPluginsDirectory() const
{
    return m_pluginsDirectory;
}

std::vector<std::filesystem::path> PluginManager::DiscoverPlugins()
{
    std::vector<std::filesystem::path> plugins;

    if (!std::filesystem::exists(m_pluginsDirectory))
    {
        util::Logger::Warn("PluginManager: Plugins directory does not exist: {}",
            m_pluginsDirectory.string());
        return plugins;
    }

    util::Logger::Info("PluginManager: Discovering plugins in: {}",
        m_pluginsDirectory.string());

    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(m_pluginsDirectory))
        {
            if (!entry.is_regular_file())
                continue;

            const auto& path = entry.path();
            std::string extension = path.extension().string();

#ifdef _WIN32
            if (extension == ".dll")
#elif defined(__APPLE__)
            if (extension == ".dylib")
#else
            if (extension == ".so")
#endif
            {
                plugins.push_back(path);
                util::Logger::Debug("PluginManager: Found plugin: {}", path.string());
            }
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("PluginManager: Error discovering plugins: {}", e.what());
    }

    util::Logger::Info("PluginManager: Discovered {} plugins", plugins.size());
    return plugins;
}

util::Result<std::string, error::Error> PluginManager::LoadPlugin(
    const std::filesystem::path& pluginPath)
{
    util::Logger::Info("PluginManager: Loading plugin from: {}", pluginPath.string());

    if (!std::filesystem::exists(pluginPath))
    {
        return util::Result<std::string, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument,
                "Plugin file does not exist: " + pluginPath.string()));
    }

    // Load the plugin library
    auto result = LoadPluginLibrary(pluginPath);
    if (result.isErr())
    {
        return util::Result<std::string, error::Error>::Err(result.error());
    }

    auto plugin = result.unwrap();
    auto metadata = plugin->GetMetadata();

    // Check if plugin already loaded
    if (m_plugins.find(metadata.id) != m_plugins.end())
    {
        return util::Result<std::string, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                "Plugin already loaded: " + metadata.id));
    }

    // Initialize plugin
    if (!plugin->Initialize())
    {
        return util::Result<std::string, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                "Plugin initialization failed: " + plugin->GetLastError()));
    }

    // Store plugin info
    PluginLoadInfo info;
    info.pluginId = metadata.id;
    info.path = pluginPath;
    info.instance = std::move(plugin);
    info.enabled = false;  // Plugins start disabled, must be explicitly enabled
    info.autoLoad = false; // Default to not auto-load

    // Apply cached configuration if available
    auto cacheIt = m_configCache.find(metadata.id);
    if (cacheIt != m_configCache.end()) {
        // Only restore autoLoad flag - enabled state will be set by EnablePlugin()
        // which properly notifies observers
        info.autoLoad = cacheIt->second.autoLoad;
        util::Logger::Info("PluginManager: Applied cached config to {}: autoLoad={}", 
            metadata.id, info.autoLoad);
    }

    m_plugins[metadata.id] = std::move(info);

    // Notify observers about plugin load
    // Note: FieldConversionPluginRegistry will handle registration via observer pattern
    NotifyObservers(PluginEvent::Loaded, metadata.id, m_plugins[metadata.id].instance.get());

    util::Logger::Info("PluginManager: Successfully loaded plugin: {} ({})",
        metadata.name, metadata.id);

    return util::Result<std::string, error::Error>::Ok(metadata.id);
}

util::Result<std::unique_ptr<IPlugin>, error::Error> PluginManager::LoadPluginLibrary(
    const std::filesystem::path& path)
{
#ifdef _WIN32
    HMODULE handle = LoadLibraryW(path.wstring().c_str());
    if (!handle)
    {
        DWORD error = GetLastError();
        return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                "Failed to load library: " + std::to_string(error)));
    }

    auto createFunc = reinterpret_cast<PluginFactoryFunc>(
        GetProcAddress(handle, "CreatePlugin"));
    if (!createFunc)
    {
        FreeLibrary(handle);
        return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                "CreatePlugin function not found"));
    }
#else
    void* handle = dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle)
    {
        const char* error = dlerror();
        return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                std::string("Failed to load library: ") + (error ? error : "unknown")));
    }

    auto createFunc = reinterpret_cast<PluginFactoryFunc>(
        dlsym(handle, "CreatePlugin"));
    if (!createFunc)
    {
        const char* error = dlerror();
        dlclose(handle);
        return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                std::string("CreatePlugin function not found: ") + (error ? error : "unknown")));
    }
#endif

    try
    {
        auto plugin = createFunc();
        if (!plugin)
        {
            UnloadLibrary(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError,
                    "CreatePlugin returned nullptr"));
        }

        // Store library handle in plugin info (will be done by caller)
        return util::Result<std::unique_ptr<IPlugin>, error::Error>::Ok(std::move(plugin));
    }
    catch (const std::exception& e)
    {
        UnloadLibrary(handle);
        return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                std::string("Exception creating plugin: ") + e.what()));
    }
}

void PluginManager::UnloadLibrary(void* handle)
{
    if (!handle)
        return;

#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

util::Result<bool, error::Error> PluginManager::UnloadPlugin(const std::string& pluginId)
{
    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end())
    {
        return util::Result<bool, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument,
                "Plugin not found: " + pluginId));
    }

    util::Logger::Info("PluginManager: Unloading plugin: {}", pluginId);

    auto& info = it->second;
    if (info.instance)
    {
        info.instance->Shutdown();
    }
    if (info.libraryHandle)
    {
        UnloadLibrary(info.libraryHandle);
    }

    // Notify observers before unloading
    NotifyObservers(PluginEvent::Unloaded, pluginId, nullptr);

    m_plugins.erase(it);

    util::Logger::Info("PluginManager: Plugin unloaded: {}", pluginId);
    return util::Result<bool, error::Error>::Ok(true);
}

util::Result<std::string, error::Error> PluginManager::RegisterPlugin(
    const std::filesystem::path& sourcePath)
{
    util::Logger::Info("PluginManager: Registering plugin from: {}", sourcePath.string());

    if (!std::filesystem::exists(sourcePath))
    {
        return util::Result<std::string, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument,
                "Plugin file does not exist: " + sourcePath.string()));
    }

    try {
        // Create plugins directory if it doesn't exist
        if (!std::filesystem::exists(m_pluginsDirectory)) {
            std::filesystem::create_directories(m_pluginsDirectory);
        }
        
        // Copy plugin file to user directory
        std::filesystem::path targetPath = m_pluginsDirectory / sourcePath.filename();
        std::filesystem::copy_file(sourcePath, targetPath, 
            std::filesystem::copy_options::overwrite_existing);
        
        util::Logger::Info("PluginManager: Copied plugin from {} to {}",
            sourcePath.string(), targetPath.string());
        
        // Load the plugin to get its ID
        auto loadResult = LoadPlugin(targetPath);
        if (loadResult.isErr()) {
            // Clean up copied file if load failed
            std::filesystem::remove(targetPath);
            return util::Result<std::string, error::Error>::Err(loadResult.error());
        }
        
        std::string pluginId = loadResult.unwrap();
        m_registeredPlugins[pluginId] = targetPath;
        
        // Notify observers about plugin registration
        NotifyObservers(PluginEvent::Registered, pluginId, m_plugins[pluginId].instance.get());
        
        util::Logger::Info("PluginManager: Plugin registered successfully: {}", pluginId);
        return util::Result<std::string, error::Error>::Ok(pluginId);
    }
    catch (const std::exception& ex) {
        return util::Result<std::string, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                std::string("Failed to register plugin: ") + ex.what()));
    }
}

util::Result<bool, error::Error> PluginManager::UnregisterPlugin(const std::string& pluginId)
{
    util::Logger::Info("PluginManager: Unregistering plugin: {}", pluginId);
    
    bool wasLoaded = false;
    bool wasRegistered = false;
    
    // First disable/unload if currently loaded
    if (m_plugins.find(pluginId) != m_plugins.end()) {
        wasLoaded = true;
        auto disableResult = DisablePlugin(pluginId);
        if (disableResult.isErr()) {
            util::Logger::Warn("PluginManager: Failed to disable plugin during unregister: {}",
                disableResult.error().what());
        }
        
        // Also unload it completely
        auto unloadResult = UnloadPlugin(pluginId);
        if (unloadResult.isErr()) {
            util::Logger::Warn("PluginManager: Failed to unload plugin during unregister: {}",
                unloadResult.error().what());
        }
    }
    
    // Remove from registered plugins and delete file
    auto it = m_registeredPlugins.find(pluginId);
    if (it != m_registeredPlugins.end()) {
        wasRegistered = true;
        std::filesystem::path pluginPath = it->second;
        
        try {
            if (std::filesystem::exists(pluginPath)) {
                std::filesystem::remove(pluginPath);
                util::Logger::Info("PluginManager: Removed plugin file: {}", pluginPath.string());
            }
            m_registeredPlugins.erase(it);
        } catch (const std::exception& ex) {
            util::Logger::Error("PluginManager: Failed to remove plugin file: {}", ex.what());
            // Continue anyway - we'll still remove from registry and notify
        }
    }
    
    // If the plugin was neither loaded nor registered, return error
    if (!wasLoaded && !wasRegistered) {
        return util::Result<bool, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument,
                "Plugin not found: " + pluginId));
    }
    
    // Notify observers about plugin unregistration
    NotifyObservers(PluginEvent::Unregistered, pluginId, nullptr);
    
    util::Logger::Info("PluginManager: Plugin unregistered successfully: {}", pluginId);
    return util::Result<bool, error::Error>::Ok(true);
}

IPlugin* PluginManager::GetPlugin(const std::string& pluginId)
{
    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end())
    {
        return nullptr;
    }
    return it->second.instance.get();
}

const std::map<std::string, PluginLoadInfo>& PluginManager::GetLoadedPlugins() const
{
    return m_plugins;
}

std::vector<std::string> PluginManager::GetPluginsByType(PluginType type) const
{
    std::vector<std::string> result;
    
    for (const auto& [id, info] : m_plugins)
    {
        if (info.instance && info.instance->GetMetadata().type == type)
        {
            result.push_back(id);
        }
    }
    
    return result;
}

util::Result<bool, error::Error> PluginManager::EnablePlugin(const std::string& pluginId)
{
    util::Logger::Info("PluginManager: Enabling plugin: {}", pluginId);
    
    // Check if already loaded
    auto it = m_plugins.find(pluginId);
    if (it != m_plugins.end()) {
        auto& info = it->second;
        if (info.enabled) {
            util::Logger::Info("PluginManager: Plugin already enabled: {}", pluginId);
            return util::Result<bool, error::Error>::Ok(true);
        }
        
        // Plugin is loaded but disabled - just re-enable it
        info.enabled = true;
        
        // Notify observers about plugin enable
        NotifyObservers(PluginEvent::Enabled, pluginId, info.instance.get());
        
        util::Logger::Info("PluginManager: Plugin re-enabled: {}", pluginId);
        return util::Result<bool, error::Error>::Ok(true);
    }
    
    // Plugin not loaded - need to find and load it
    auto regIt = m_registeredPlugins.find(pluginId);
    if (regIt == m_registeredPlugins.end()) {
        return util::Result<bool, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument,
                "Plugin not registered: " + pluginId));
    }
    
    // Load the plugin
    auto loadResult = LoadPlugin(regIt->second);
    if (loadResult.isErr()) {
        return util::Result<bool, error::Error>::Err(loadResult.error());
    }
    
    // Mark as enabled
    auto& info = m_plugins[pluginId];
    info.enabled = true;
    
    // Notify observers about plugin enable
    NotifyObservers(PluginEvent::Enabled, pluginId, info.instance.get());
    
    // Save configuration to persist enabled state
    SaveConfiguration();
    
    util::Logger::Info("PluginManager: Plugin enabled: {}", pluginId);
    return util::Result<bool, error::Error>::Ok(true);
}

util::Result<bool, error::Error> PluginManager::DisablePlugin(const std::string& pluginId)
{
    util::Logger::Info("PluginManager: Disabling plugin: {}", pluginId);
    
    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end())
    {
        util::Logger::Warn("PluginManager: Plugin not loaded: {}", pluginId);
        return util::Result<bool, error::Error>::Ok(true);  // Already disabled
    }

    if (!it->second.enabled) {
        util::Logger::Info("PluginManager: Plugin already disabled: {}", pluginId);
        return util::Result<bool, error::Error>::Ok(true);
    }

    it->second.enabled = false;
    
    // Notify observers about plugin disable
    NotifyObservers(PluginEvent::Disabled, pluginId, it->second.instance.get());
    
    // NOTE: We keep the plugin loaded in memory, just mark it as disabled
    // This allows quick re-enabling without reloading from disk
    
    // Save configuration to persist disabled state
    SaveConfiguration();
    
    util::Logger::Info("PluginManager: Plugin disabled: {}", pluginId);
    return util::Result<bool, error::Error>::Ok(true);
}

void PluginManager::SetPluginAutoLoad(const std::string& pluginId, bool autoLoad)
{
    auto it = m_plugins.find(pluginId);
    if (it != m_plugins.end())
    {
        it->second.autoLoad = autoLoad;
        SaveConfiguration();
    }
}

void PluginManager::RegisterObserver(IPluginObserver* observer)
{
    if (observer)
    {
        m_observers.push_back(observer);
        util::Logger::Debug("PluginManager: Observer registered");
    }
}

void PluginManager::UnregisterObserver(IPluginObserver* observer)
{
    auto it = std::find(m_observers.begin(), m_observers.end(), observer);
    if (it != m_observers.end())
    {
        m_observers.erase(it);
        util::Logger::Debug("PluginManager: Observer unregistered");
    }
}

void PluginManager::NotifyObservers(PluginEvent event, const std::string& pluginId, IPlugin* plugin)
{
    for (auto* observer : m_observers)
    {
        if (observer)
        {
            observer->OnPluginEvent(event, pluginId, plugin);
        }
    }
}

util::Result<bool, error::Error> PluginManager::LoadConfiguration()
{
    try {
        // Use the same directory as main config
        auto configDir = config::GetConfig().GetDefaultAppPath();
        m_configPath = configDir / "plugins_config.json";
        
        if (!std::filesystem::exists(m_configPath)) {
            util::Logger::Debug("PluginManager: No plugin configuration file found at {}", 
                m_configPath.string());
            return util::Result<bool, error::Error>::Ok(true);
        }
        
        std::ifstream file(m_configPath);
        if (!file.is_open()) {
            return util::Result<bool, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError,
                    "Failed to open plugin configuration file: " + m_configPath.string()));
        }
        
        nlohmann::json config;
        file >> config;
        file.close();
        
        // Cache plugin states (plugins might not be loaded yet)
        if (config.contains("plugins") && config["plugins"].is_array()) {
            for (const auto& pluginConfig : config["plugins"]) {
                if (!pluginConfig.contains("id")) continue;
                
                std::string pluginId = pluginConfig["id"];
                bool enabled = pluginConfig.value("enabled", false);
                bool autoLoad = pluginConfig.value("autoLoad", false);
                
                // Cache the configuration
                m_configCache[pluginId] = {enabled, autoLoad};
                util::Logger::Info("PluginManager: Cached config for {}: enabled={}, autoLoad={}", 
                    pluginId, enabled, autoLoad);
                
                // If plugin is already loaded, update its state immediately
                auto it = m_plugins.find(pluginId);
                if (it != m_plugins.end()) {
                    it->second.enabled = enabled;
                    it->second.autoLoad = autoLoad;
                    util::Logger::Info("PluginManager: Applied cached state to loaded plugin: {}", 
                        pluginId);
                }
            }
        }
        
        util::Logger::Info("PluginManager: Configuration loaded from {}", m_configPath.string());
        return util::Result<bool, error::Error>::Ok(true);
        
    } catch (const std::exception& e) {
        return util::Result<bool, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                std::string("Failed to load plugin configuration: ") + e.what()));
    }
}

util::Result<bool, error::Error> PluginManager::SaveConfiguration()
{
    try {
        // Use the same directory as main config
        auto configDir = config::GetConfig().GetDefaultAppPath();
        m_configPath = configDir / "plugins_config.json";
        
        // Ensure directory exists
        if (!std::filesystem::exists(configDir)) {
            std::filesystem::create_directories(configDir);
        }
        
        nlohmann::json config;
        config["version"] = "1.0";
        config["plugins"] = nlohmann::json::array();
        
        // Save all plugin states
        for (const auto& [pluginId, info] : m_plugins) {
            nlohmann::json pluginConfig;
            pluginConfig["id"] = pluginId;
            pluginConfig["enabled"] = info.enabled;
            pluginConfig["autoLoad"] = info.autoLoad;
            pluginConfig["path"] = info.path.string();
            
            config["plugins"].push_back(pluginConfig);
        }
        
        std::ofstream file(m_configPath);
        if (!file.is_open()) {
            return util::Result<bool, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError,
                    "Failed to open plugin configuration file for writing: " + m_configPath.string()));
        }
        
        file << config.dump(2);
        file.close();
        
        util::Logger::Info("PluginManager: Configuration saved to {}", m_configPath.string());
        return util::Result<bool, error::Error>::Ok(true);
        
    } catch (const std::exception& e) {
        return util::Result<bool, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                std::string("Failed to save plugin configuration: ") + e.what()));
    }
}

util::Result<bool, error::Error> PluginManager::ValidateDependencies() const
{
    for (const auto& [id, info] : m_plugins)
    {
        if (!info.instance)
            continue;

        auto metadata = info.instance->GetMetadata();
        for (const auto& dep : metadata.dependencies)
        {
            if (m_plugins.find(dep) == m_plugins.end())
            {
                return util::Result<bool, error::Error>::Err(
                    error::Error(error::ErrorCode::RuntimeError,
                        "Missing dependency: " + dep + " required by " + id));
            }
        }
    }

    return util::Result<bool, error::Error>::Ok(true);
}

} // namespace plugin
