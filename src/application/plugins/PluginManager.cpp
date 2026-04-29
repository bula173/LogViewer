/**
 * @file PluginManager.cpp
 * @brief Implementation of plugin management system
 * @author LogViewer Development Team
 * @date 2025
 */

#include "PluginManager.hpp"
#include "Config.hpp"
#include "FieldConversionPluginRegistry.hpp"
#include "Logger.hpp"
#include "../../plugin_api/PluginLoggerC.h"
#include "../../plugin_api/PluginKeyEncryptionC.h"
#include "../../plugin_api/PluginC.h"
#include "../../plugin_api/PluginHostUiC.h"
#include "../util/KeyEncryption.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
#include <string_view>
#include <cstdio>
#include <QStandardPaths>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

// Application-provided C callbacks for plugins (registered at startup)
extern "C" void AppPluginLog(int level, const char* message)
{
    if (!message) return;
    switch (level) {
        case PLUGIN_LOG_TRACE:
        case PLUGIN_LOG_DEBUG: util::Logger::Debug("[plugin] {}", message); break;
        case PLUGIN_LOG_INFO: util::Logger::Info("[plugin] {}", message); break;
        case PLUGIN_LOG_WARN: util::Logger::Warn("[plugin] {}", message); break;
        case PLUGIN_LOG_ERROR: util::Logger::Error("[plugin] {}", message); break;
        case PLUGIN_LOG_CRITICAL: util::Logger::Critical("[plugin] {}", message); break;
        default: util::Logger::Info("[plugin] {}", message); break;
    }
}

extern "C" char* AppPluginKeyEncode(const char* input)
{
    if (!input) return nullptr;
    try {
        auto out = util::KeyEncryption::Encrypt(std::string(input));
        char* ret = (char*)std::malloc(out.size() + 1);
        if (!ret) return nullptr;
        memcpy(ret, out.c_str(), out.size() + 1);
        return ret;
    } catch (...) { return nullptr; }
}

extern "C" char* AppPluginKeyDecode(const char* input)
{
    if (!input) return nullptr;
    try {
        auto out = util::KeyEncryption::Decrypt(std::string(input));
        char* ret = (char*)std::malloc(out.size() + 1);
        if (!ret) return nullptr;
        memcpy(ret, out.c_str(), out.size() + 1);
        return ret;
    } catch (...) { return nullptr; }
}

namespace plugin
{

static constexpr int kSupportedApiMajor = 1;
static constexpr int kSupportedApiMinMinor = 0;
static constexpr int kSupportedApiMaxMinor = 1;

namespace {
    struct VersionParts {
        int major {0};
        int minor {0};
        int patch {0};
        bool ok {false};
    };

    VersionParts ParseVersion(std::string_view v) {
        VersionParts parts;
        int a = 0, b = 0, c = 0;
        if (std::sscanf(std::string(v).c_str(), "%d.%d.%d", &a, &b, &c) == 3) {
            parts.major = a;
            parts.minor = b;
            parts.patch = c;
            parts.ok = true;
        }
        return parts;
    }

    bool IsApiVersionCompatible(std::string_view version) {
        auto parts = ParseVersion(version);
        if (!parts.ok) return false;
        if (parts.major != kSupportedApiMajor) return false;
        return parts.minor >= kSupportedApiMinMinor && parts.minor <= kSupportedApiMaxMinor;
    }
}

// Helper: resolve extraction directory (either input dir or appdata plugins/<stem>)
// Forward declarations for helper functions defined lower in this file
namespace {
    bool ExtractZipPlugin(const std::filesystem::path& zipPath, const std::filesystem::path& extractDir);
    void* LoadLibraryHelper(const std::filesystem::path& path);
}

// Helper: resolve extraction directory (either input dir or appdata plugins/<stem>)
static util::Result<std::filesystem::path, error::Error> ResolvePluginExtractDir(const std::filesystem::path& pluginPath)
{
    if (std::filesystem::is_directory(pluginPath)) {
        return util::Result<std::filesystem::path, error::Error>::Ok(pluginPath);
    }

    auto appDataPath = config::GetConfig().GetDefaultAppPath();
    auto extractDir = appDataPath / "plugins" / pluginPath.stem();
    if (!ExtractZipPlugin(pluginPath, extractDir)) {
        return util::Result<std::filesystem::path, error::Error>::Err(
            error::Error("Failed to extract plugin ZIP: " + pluginPath.string()));
    }
    return util::Result<std::filesystem::path, error::Error>::Ok(extractDir);
}

// Helper: load dependency DLLs from a plugin lib folder.
// DLLs are loaded by explicit name from a "deps.txt" manifest in libFolder
// (one filename per line, no path components).  If no manifest exists the
// folder is skipped so we never enumerate and load every .dll we find —
// that pattern (SetDllDirectoryW + directory_iterator + LoadLibrary on all
// .dll files) is indistinguishable from DLL-hijacking/sideloading to AV.
static std::vector<void*> LoadDependencyHandles(const std::filesystem::path& libFolder)
{
    std::vector<void*> handles;
    if (!std::filesystem::exists(libFolder)) return handles;

#ifdef _WIN32
    // Read the explicit dependency manifest
    const auto manifestPath = libFolder / "deps.txt";
    if (!std::filesystem::exists(manifestPath))
    {
        util::Logger::Debug("PluginManager: No deps.txt in {}, skipping dependency load",
                            libFolder.string());
        return handles;
    }

    std::ifstream manifest(manifestPath);
    std::string dllName;

    // Set the plugin lib folder as the DLL search directory for the
    // duration of the explicit loads, then restore immediately.
    SetDllDirectoryW(libFolder.wstring().c_str());

    while (std::getline(manifest, dllName))
    {
        // Strip whitespace / CR
        while (!dllName.empty() && (dllName.back() == '\r' || dllName.back() == '\n' || dllName.back() == ' '))
            dllName.pop_back();
        if (dllName.empty() || dllName[0] == '#') continue;

        // Only allow bare filenames — reject any path separators
        if (dllName.find('/') != std::string::npos ||
            dllName.find('\\') != std::string::npos)
        {
            util::Logger::Warn("PluginManager: deps.txt entry '{}' contains path separator — skipped",
                               dllName);
            continue;
        }

        const auto dllPath = libFolder / dllName;
        void* handle = LoadLibraryHelper(dllPath);
        if (handle)
        {
            handles.push_back(handle);
            util::Logger::Info("PluginManager: Loaded dependency: {}", dllName);
        }
        else
        {
            util::Logger::Warn("PluginManager: Failed to load listed dependency '{}' — "
                               "main plugin may still work if the DLL is on PATH",
                               dllName);
        }
    }

    SetDllDirectoryW(NULL);
#endif // _WIN32
    return handles;
}

// Helper: probe optional C-style plugin exports into PluginLoadInfo
static void ProbeOptionalPluginExports(void* libHandle, PluginLoadInfo& info)
{
#ifdef _WIN32
    auto probe_sym = [&](const char* name)->void* { return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(libHandle), name)); };
#else
    auto probe_sym = [&](const char* name)->void* { return dlsym(libHandle, name); };
#endif

    // Probe for generic panel creation exports
    info.pluginCreateLeftPanel = probe_sym("Plugin_CreateLeftPanel");
    info.pluginCreateRightPanel = probe_sym("Plugin_CreateRightPanel");
    info.pluginCreateBottomPanel = probe_sym("Plugin_CreateBottomPanel");
    info.pluginCreateMainPanel = probe_sym("Plugin_CreateMainPanel");
    info.pluginCreateAIService = probe_sym("Plugin_CreateAIService");
    info.pluginDestroyAIService = probe_sym("Plugin_DestroyAIService");
    info.pluginSetAIEventsContainer = probe_sym("Plugin_SetAIEventsContainer");
    info.pluginSetEventsCallbacks = probe_sym("Plugin_SetEventsCallbacks");
    info.pluginSetHostUiCallbacks = probe_sym("Plugin_SetHostUiCallbacks");
    info.pluginDestroyPanelWidget = probe_sym("Plugin_DestroyPanelWidget");

    util::Logger::Debug("PluginManager: Probed panel exports for {}: left={} right={} bottom={} main={} destroy={}",
        info.pluginId,
        info.pluginCreateLeftPanel != nullptr,
        info.pluginCreateRightPanel != nullptr,
        info.pluginCreateBottomPanel != nullptr,
        info.pluginCreateMainPanel != nullptr,
        info.pluginDestroyPanelWidget != nullptr);

    util::Logger::Debug("PluginManager: Probed events setter for {}: setEvents={} setCallbacks={}",
        info.pluginId,
        info.pluginSetAIEventsContainer != nullptr,
        info.pluginSetEventsCallbacks != nullptr);

    util::Logger::Debug("PluginManager: Probed host UI callbacks setter for {}: setHostUiCallbacks={}",
        info.pluginId,
        info.pluginSetHostUiCallbacks != nullptr);
    util::Logger::Debug("PluginManager: Probed AI service exports for {}: create={} destroy={}",
        info.pluginId, info.pluginCreateAIService != nullptr, info.pluginDestroyAIService != nullptr);
}


// Helper functions - must be defined before use
namespace {
    std::optional<nlohmann::json> ParseManifest(const std::filesystem::path& manifestPath) {
        try {
            std::ifstream file(manifestPath);
            if (!file.is_open()) {
                return std::nullopt;
            }
            nlohmann::json manifest;
            file >> manifest;
            return manifest;
        } catch (const std::exception& e) {
            util::Logger::Error("Failed to parse manifest {}: {}", manifestPath.string(), e.what());
            return std::nullopt;
        }
    }

    void* LoadLibraryHelper(const std::filesystem::path& path) {
        util::Logger::Debug("Loading plugin library: {}", path.string());
        #ifdef _WIN32
            // Use the wide-character API to avoid ANSI path truncation issues
            // and prefer DLLs next to the plugin by using LOAD_WITH_ALTERED_SEARCH_PATH.
            util::Logger::Debug("Using LoadLibraryExW with altered search path for: {}", path.string());
            HMODULE h = LoadLibraryExW(path.wstring().c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
            if (!h) {
                DWORD err = GetLastError();
                LPSTR msgBuf = nullptr;
                FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msgBuf, 0, NULL);
                std::string errMsg = msgBuf ? std::string(msgBuf) : "Unknown error";
                if (msgBuf) LocalFree(msgBuf);
                util::Logger::Warn("LoadLibraryExW failed for {}: {} (code={})", path.string(), errMsg, err);
            } else {
                util::Logger::Debug("LoadLibraryExW succeeded for: {}", path.string());
            }
            return static_cast<void*>(h);
        #else
            util::Logger::Debug("Using dlopen for: {}", path.string());
            return dlopen(path.string().c_str(), RTLD_LAZY);
        #endif
    }

    bool ExtractZipPlugin(const std::filesystem::path& zipPath, 
                         const std::filesystem::path& extractDir) {
        // Create extraction directory
        if (std::filesystem::exists(extractDir)) {
            std::filesystem::remove_all(extractDir);
        }
        std::filesystem::create_directories(extractDir);
        
        struct archive* a = archive_read_new();
        archive_read_support_format_zip(a);
        
        if (archive_read_open_filename(a, zipPath.string().c_str(), 10240) != ARCHIVE_OK) {
            util::Logger::Error("Failed to open ZIP: {}", zipPath.string());
            archive_read_free(a);
            return false;
        }
        
        struct archive_entry* entry;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            const char* rawName = archive_entry_pathname(entry);
            if (!rawName) { archive_read_data_skip(a); continue; }

            // Guard against Zip Slip: resolve the candidate path and verify it
            // stays inside extractDir before writing anything to disk.
            std::filesystem::path outputPath = std::filesystem::weakly_canonical(
                extractDir / std::filesystem::path(rawName).lexically_normal());

            // Reject any entry whose resolved path escapes the extraction root.
            // Use a prefix check on the canonical string representations rather
            // than relative() + string-contains(".."), which can be bypassed by
            // symlinks or platform-specific path representations.
            auto extractDirCanonical = std::filesystem::weakly_canonical(extractDir);
            const std::string extractDirStr = extractDirCanonical.string() + "/";
            if (outputPath.string().substr(0,
                    std::min(outputPath.string().size(), extractDirStr.size())) != extractDirStr
                && outputPath != extractDirCanonical)
            {
                util::Logger::Warn("ExtractZipPlugin: Skipping path-traversal entry: {}", rawName);
                archive_read_data_skip(a);
                continue;
            }

            if (archive_entry_filetype(entry) == AE_IFDIR) {
                std::filesystem::create_directories(outputPath);
            } else {
                if (auto parent = outputPath.parent_path(); !parent.empty()) {
                    std::filesystem::create_directories(parent);
                }
                std::ofstream outFile(outputPath, std::ios::binary);
                if (!outFile.is_open()) {
                    util::Logger::Error("ExtractZipPlugin: Failed to open output file: {}", outputPath.string());
                    archive_read_data_skip(a);
                    continue;
                }
                const void* buff;
                size_t size;
                int64_t offset;

                while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
                    outFile.write(static_cast<const char*>(buff), static_cast<std::streamsize>(size));
                }
                outFile.close();
            }
        }
        
        archive_read_free(a);
        util::Logger::Info("Extracted plugin from: {}", zipPath.string());
        return true;
    }
}

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

    // Register application logger/encryption callbacks for plugins (C API)
    static bool callbacksRegistered = false;
    if (!callbacksRegistered) {
        PluginLogger_Register(&AppPluginLog);
        PluginKeyEncryption_Register(&AppPluginKeyEncode, &AppPluginKeyDecode);
        callbacksRegistered = true;
        util::Logger::Debug("PluginManager: Registered plugin API callbacks");
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
            // Ensure the plugin instance is destroyed while the library is
            // still loaded so any C-ABI destructor callbacks remain valid.
            info.instance->Shutdown();
            info.instance.reset();
        }
        if (info.libraryHandle)
        {
            UnloadLibrary(info.libraryHandle);
            info.libraryHandle = nullptr;
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
            if (!entry.is_regular_file() && !entry.is_directory())
                continue;

            const auto& path = entry.path();
            // Only consider extracted plugin directories as discoverable plugins.
            // ZIP archives may be present but we only act on directories that
            // contain a plugin payload (config.json). This avoids double-loading
            // or extracting archives unintentionally.
            if (std::filesystem::is_directory(path)) {
                auto manifest = path / "config.json";
                if (std::filesystem::exists(manifest)) {
                    plugins.push_back(path);
                    util::Logger::Debug("PluginManager: Found extracted plugin: {}", path.string());
                } else {
                    util::Logger::Debug("PluginManager: Ignoring directory without config.json: {}", path.string());
                }
            } else {
                util::Logger::Debug("PluginManager: Ignoring non-directory plugin entry: {}", path.string());
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
    if (pluginPath.extension() != ".zip" && !std::filesystem::is_directory(pluginPath)) {
        return util::Result<std::string, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument,
                "Only zipped plugins or plugin directories are supported. Expected .zip or directory: " + pluginPath.string()));
    }

    auto extractRes = ResolvePluginExtractDir(pluginPath);
    if (extractRes.isErr()) {
        return util::Result<std::string, error::Error>::Err(extractRes.error());
    }
    std::filesystem::path extractDir = extractRes.unwrap();

    // Parse config.json inside the extracted folder
    auto manifestPath = extractDir / "config.json";
    auto manifest = ParseManifest(manifestPath);
    if (!manifest) {
        return util::Result<std::string, error::Error>::Err(
            error::Error("Missing or invalid config.json in plugin: " + manifestPath.string()));
    }

    if (!manifest->contains("entry") || !(*manifest)["entry"].is_string()) {
        return util::Result<std::string, error::Error>::Err(
            error::Error("config.json missing required 'entry' field"));
    }

    if (!manifest->contains("apiVersion") || !(*manifest)["apiVersion"].is_string()) {
        return util::Result<std::string, error::Error>::Err(
            error::Error("config.json missing required 'apiVersion' field"));
    }

    const std::string manifestApiVersion = (*manifest)["apiVersion"].get<std::string>();
    if (!IsApiVersionCompatible(manifestApiVersion)) {
        return util::Result<std::string, error::Error>::Err(
            error::Error(
                "Plugin API version mismatch. Expected compatible 1.x minor (1.0-1.1), found " + manifestApiVersion));
    }

    std::filesystem::path actualPluginPath = extractDir / (*manifest)["entry"].get<std::string>();

    // Optional: validate metadata id/version if present
    if (manifest->contains("id") && !(*manifest)["id"].is_string()) {
        return util::Result<std::string, error::Error>::Err(
            error::Error("config.json field 'id' must be string"));
    }

    util::Logger::Info("PluginManager: Loading plugin from: {}", actualPluginPath.string());

    if (!std::filesystem::exists(actualPluginPath))
    {
        return util::Result<std::string, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument,
                "Plugin file does not exist: " + actualPluginPath.string()));
    }

    // Removed dependency handling logic as there will be no dependencies in the JSON config
    
    // Load dependencies from plugin/lib folder. Temporarily set the DLL
    // search directory to the plugin lib folder so Windows prefers DLLs
    // shipped with the plugin over those found on PATH.
    std::filesystem::path libFolder = extractDir / "lib";
    if (std::filesystem::exists(libFolder)) {
        util::Logger::Debug("PluginManager: Loading dependencies from {}", libFolder.string());
        auto handles = LoadDependencyHandles(libFolder);
        // Append to manager's dependency handle list
        for (auto h : handles) m_dependencyHandles.push_back(h);
    } else {
        util::Logger::Warn("Plugin lib folder does not exist: {}", libFolder.string());
    }
    
    // Load the plugin library, passing expected API version from manifest
    void* libHandle = nullptr;
    void* pluginOpaque = nullptr;
    auto result = LoadPluginLibrary(actualPluginPath, manifestApiVersion, &libHandle, &pluginOpaque);
    if (result.isErr())
    {
        return util::Result<std::string, error::Error>::Err(result.error());
    }

    auto plugin = result.unwrap();
    auto metadata = plugin->GetMetadata();

    if (metadata.apiVersion != manifestApiVersion) {
        return util::Result<std::string, error::Error>::Err(
            error::Error("Plugin binary API version does not match manifest: " + metadata.apiVersion +
                " vs " + manifestApiVersion));
    }

    if (!IsApiVersionCompatible(metadata.apiVersion)) {
        return util::Result<std::string, error::Error>::Err(
            error::Error("Plugin API version incompatible with application. Expected 1.x minor (1.0-1.1), got " + metadata.apiVersion));
    }

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
    info.path = actualPluginPath;
    info.instance = std::move(plugin);
    info.libraryHandle = libHandle;
    info.pluginOpaqueHandle = pluginOpaque;
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

    // Remember package path so unregister can remove it after restart
    m_registeredPlugins[metadata.id] = pluginPath;

    // Probe for optional plugin C-exports and store pointers
    if (info.libraryHandle) {
        ProbeOptionalPluginExports(info.libraryHandle, info);
    }

    // Provide host UI callbacks early if the plugin supports it.
    // (Even though the plugin may be disabled, this avoids timing issues.)
    if (info.pluginOpaqueHandle && info.pluginSetHostUiCallbacks && m_hostUiOpaque && m_hostUiCallbacks.size >= sizeof(PluginHostUiCallbacks)) {
        using SetHostUiCallbacksFn = void(*)(void*, void*, const PluginHostUiCallbacks*);
        auto fn = reinterpret_cast<SetHostUiCallbacksFn>(info.pluginSetHostUiCallbacks);
        try { fn(info.pluginOpaqueHandle, m_hostUiOpaque, &m_hostUiCallbacks); } catch(...) {}
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
    const std::filesystem::path& path,
    const std::string& expectedApiVersion,
    void** outLibraryHandle,
    void** outPluginOpaqueHandle)
{
#ifdef _WIN32
    // Load the plugin using LoadLibraryEx with altered search path so the
    // plugin's directory is searched first for its dependent DLLs. This
    // avoids picking up conflicting DLLs from global locations (e.g. Qt
    // installations on the system PATH).
    HMODULE handle = LoadLibraryExW(path.wstring().c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!handle)
    {
        DWORD error = GetLastError();
        LPSTR messageBuffer = nullptr;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
        std::string errorMessage = (size > 0 && messageBuffer) ? std::string(messageBuffer, size) : "Unknown error";
        if (messageBuffer) LocalFree(messageBuffer);

        // Log directory contents to aid debugging, but only in debug builds.
        // On Windows release builds, enumerating a directory immediately after
        // a failed LoadLibraryExW looks like searching for alternate injection
        // targets — an AV heuristic trigger we deliberately avoid.
#ifndef NDEBUG
        try {
            auto dir = path.parent_path();
            util::Logger::Debug("PluginManager: Listing files in plugin directory {}:", dir.string());
            for (const auto& e : std::filesystem::directory_iterator(dir)) {
                util::Logger::Debug(" - {}", e.path().string());
            }
        } catch (const std::exception& e) {
            util::Logger::Debug("PluginManager: Failed to list plugin directory: {}", std::string(e.what()));
        }
#endif

        return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                "Failed to load library: " + errorMessage + " (code: " + std::to_string(error) + ")"));
    }

    // Optional pre-flight: query the plugin binary for a reported API version
    auto getBinVer = reinterpret_cast<const char*(*)()>(
        GetProcAddress(handle, "GetPluginBinaryApiVersion"));
    if (getBinVer)
    {
        const char* reported = nullptr;
        try { reported = getBinVer(); } catch (...) { reported = nullptr; }
        if (!reported)
        {
            FreeLibrary(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError, "Plugin binary reported empty API version"));
        }
        std::string reportedStr(reported);
        if (reportedStr != expectedApiVersion)
        {
            FreeLibrary(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError,
                    "Plugin binary API version does not match manifest: " + reportedStr + " vs " + expectedApiVersion));
        }
        if (!IsApiVersionCompatible(reportedStr))
        {
            FreeLibrary(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError,
                    "Plugin binary API version incompatible with application: " + reportedStr));
        }
    }
    // Prefer C ABI plugins that export `Plugin_Create` and associated C functions.
    // If present, wrap them in a small adapter that implements `IPlugin` so
    // the rest of the application can treat SDK-first plugins like regular
    // in-tree plugins.
    auto c_create_sym = reinterpret_cast<void*(*)()>(GetProcAddress(handle, "Plugin_Create"));
    if (c_create_sym)
    {
        util::Logger::Info("PluginManager: Found C-ABI plugin exports, using C ABI path");

        using Plugin_Create_Fn = void* (*)();
        using Plugin_Destroy_Fn = void (*)(void*);
        using Plugin_GetMetadataJson_Fn = const char* (*)(void*);
        using Plugin_Initialize_Fn = bool (*)(void*);
        using Plugin_Shutdown_Fn = void (*)(void*);
        using Plugin_GetLastError_Fn = const char* (*)(void*);

        auto c_create = reinterpret_cast<Plugin_Create_Fn>(c_create_sym);
        auto c_destroy = reinterpret_cast<Plugin_Destroy_Fn>(GetProcAddress(handle, "Plugin_Destroy"));
        auto c_getMeta = reinterpret_cast<Plugin_GetMetadataJson_Fn>(GetProcAddress(handle, "Plugin_GetMetadataJson"));
        auto c_initialize = reinterpret_cast<Plugin_Initialize_Fn>(GetProcAddress(handle, "Plugin_Initialize"));
        auto c_shutdown = reinterpret_cast<Plugin_Shutdown_Fn>(GetProcAddress(handle, "Plugin_Shutdown"));
        auto c_getLastErr = reinterpret_cast<Plugin_GetLastError_Fn>(GetProcAddress(handle, "Plugin_GetLastError"));

        if (!c_getMeta)
        {
            util::Logger::Error("PluginManager: C-ABI plugin missing Plugin_GetMetadataJson, rejecting plugin");
            FreeLibrary(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError, "C-ABI plugin missing Plugin_GetMetadataJson"));
        }

        // Create an instance via C ABI
        void* pluginHandle = nullptr;
        try { pluginHandle = c_create(); } catch (...) { pluginHandle = nullptr; }
        if (!pluginHandle)
        {
            FreeLibrary(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError, "C-ABI Plugin_Create returned null"));
        }

        // Set logger callback immediately after plugin creation
        using Plugin_SetLoggerCallback_Fn = void (*)(void*, PluginLogFn);
        auto setLoggerSym = reinterpret_cast<Plugin_SetLoggerCallback_Fn>(GetProcAddress(handle, "Plugin_SetLoggerCallback"));
        if (setLoggerSym) {
            setLoggerSym(pluginHandle, &AppPluginLog);
            util::Logger::Info("PluginManager: Set logger callback for C-ABI plugin");
        } else {
            util::Logger::Warn("PluginManager: Plugin_SetLoggerCallback not found - plugin logs may not work");
        }

        // Adapter class implementing IPlugin by delegating to C ABI functions
        class CAbiPluginAdapter : public IPlugin {
        public:
            CAbiPluginAdapter(void* h, Plugin_Destroy_Fn destroyFn, Plugin_GetMetadataJson_Fn metaFn,
                              Plugin_Initialize_Fn initFn, Plugin_Shutdown_Fn shutdownFn,
                              Plugin_GetLastError_Fn lastErrFn)
                : handle_(h), destroyFn_(destroyFn), metaFn_(metaFn), initFn_(initFn), shutdownFn_(shutdownFn), lastErrFn_(lastErrFn)
            {
                // Populate metadata from JSON string
                try {
                    const char* js = metaFn_(handle_);
                    if (js) {
                        nlohmann::json j = nlohmann::json::parse(js);
                        PluginApi_FreeString(const_cast<char*>(js));
                        meta_.id = j.value("id", "");
                        meta_.name = j.value("name", "");
                        meta_.version = j.value("version", "");
                        meta_.apiVersion = j.value("apiVersion", "1.0.0");
                        meta_.author = j.value("author", "");
                        meta_.description = j.value("description", "");
                        meta_.website = j.value("website", "");
                        // Preserve plugin type from metadata if present
                        int t = j.value("type", static_cast<int>(PluginType::Custom));
                        meta_.type = static_cast<PluginType>(t);
                    }
                } catch (...) {}
            }
            ~CAbiPluginAdapter() override {
                if (destroyFn_ && handle_) destroyFn_(handle_);
            }
            PluginMetadata GetMetadata() const override { return meta_; }
            bool Initialize() override { return initFn_ ? initFn_(handle_) : true; }
            void Shutdown() override { if (shutdownFn_) shutdownFn_(handle_); }
            PluginStatus GetStatus() const override { return PluginStatus::Active; }
            std::string GetLastError() const override { if (lastErrFn_) { const char* s = lastErrFn_(handle_); if (s) { std::string r(s); PluginApi_FreeString(const_cast<char*>(s)); return r; } } return {}; }
            bool IsLicensed() const override { return true; }
            bool SetLicense(const std::string&) override { return true; }
            bool ValidateConfiguration() const override { return true; }
        private:
            void* handle_ = nullptr;
            Plugin_Destroy_Fn destroyFn_ = nullptr;
            Plugin_GetMetadataJson_Fn metaFn_ = nullptr;
            Plugin_Initialize_Fn initFn_ = nullptr;
            Plugin_Shutdown_Fn shutdownFn_ = nullptr;
            Plugin_GetLastError_Fn lastErrFn_ = nullptr;
            PluginMetadata meta_;
        };

        // Create adapter and return as unique_ptr<IPlugin>
        auto adapter = std::make_unique<CAbiPluginAdapter>(pluginHandle, c_destroy, c_getMeta, c_initialize, c_shutdown, c_getLastErr);
        if (outLibraryHandle) *outLibraryHandle = handle;
        if (outPluginOpaqueHandle) *outPluginOpaqueHandle = pluginHandle;
        util::Logger::Info("PluginManager: Created C-ABI plugin adapter");
        return util::Result<std::unique_ptr<IPlugin>, error::Error>::Ok(std::move(adapter));
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
    // Optional pre-flight: query GetPluginBinaryApiVersion if present
    auto getBinVerSym = dlsym(handle, "GetPluginBinaryApiVersion");
    if (getBinVerSym)
    {
        auto getBinVer = reinterpret_cast<const char*(*)()>(getBinVerSym);
        const char* reported = nullptr;
        try { reported = getBinVer(); } catch (...) { reported = nullptr; }
        if (!reported)
        {
            dlclose(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError, "Plugin binary reported empty API version"));
        }
        std::string reportedStr(reported);
        if (reportedStr != expectedApiVersion)
        {
            dlclose(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError,
                    "Plugin binary API version does not match manifest: " + reportedStr + " vs " + expectedApiVersion));
        }
        if (!IsApiVersionCompatible(reportedStr))
        {
            dlclose(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError,
                    "Plugin binary API version incompatible with application: " + reportedStr));
        }
    }

    // Require C-ABI: Plugin_Create must be exported by SDK-first plugins on Unix too.
    auto c_create_sym_unix = dlsym(handle, "Plugin_Create");
    if (!c_create_sym_unix)
    {
        const char* error = dlerror();
        dlclose(handle);
        return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
            error::Error(error::ErrorCode::RuntimeError,
                std::string("Plugin does not export required C-ABI symbol: Plugin_Create") + (error ? (std::string(" - ") + error) : "")));
    }

    // Create instance via C-ABI and wrap in adapter (Unix path)
    {
        using Plugin_Create_Fn = void* (*)();
        using Plugin_Destroy_Fn = void (*)(void*);
        using Plugin_GetMetadataJson_Fn = const char* (*)(void*);
        using Plugin_Initialize_Fn = bool (*)(void*);
        using Plugin_Shutdown_Fn = void (*)(void*);
        using Plugin_GetLastError_Fn = const char* (*)(void*);

        auto c_create = reinterpret_cast<Plugin_Create_Fn>(c_create_sym_unix);
        auto c_destroy = reinterpret_cast<Plugin_Destroy_Fn>(dlsym(handle, "Plugin_Destroy"));
        auto c_getMeta = reinterpret_cast<Plugin_GetMetadataJson_Fn>(dlsym(handle, "Plugin_GetMetadataJson"));
        auto c_initialize = reinterpret_cast<Plugin_Initialize_Fn>(dlsym(handle, "Plugin_Initialize"));
        auto c_shutdown = reinterpret_cast<Plugin_Shutdown_Fn>(dlsym(handle, "Plugin_Shutdown"));
        auto c_getLastErr = reinterpret_cast<Plugin_GetLastError_Fn>(dlsym(handle, "Plugin_GetLastError"));

        if (!c_getMeta)
        {
            util::Logger::Error("PluginManager: C-ABI plugin missing Plugin_GetMetadataJson, rejecting plugin");
            dlclose(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError, "C-ABI plugin missing Plugin_GetMetadataJson"));
        }

        void* pluginHandle = nullptr;
        try { pluginHandle = c_create(); } catch (...) { pluginHandle = nullptr; }
        if (!pluginHandle)
        {
            dlclose(handle);
            return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError, "C-ABI Plugin_Create returned null"));
        }

        // Set logger callback immediately after plugin creation
        using Plugin_SetLoggerCallback_Fn = void (*)(void*, PluginLogFn);
        auto setLoggerSym = reinterpret_cast<Plugin_SetLoggerCallback_Fn>(dlsym(handle, "Plugin_SetLoggerCallback"));
        if (setLoggerSym) {
            setLoggerSym(pluginHandle, &AppPluginLog);
            util::Logger::Info("PluginManager: Set logger callback for C-ABI plugin");
        } else {
            util::Logger::Warn("PluginManager: Plugin_SetLoggerCallback not found - plugin logs may not work");
        }

        class CAbiPluginAdapter : public IPlugin {
        public:
            CAbiPluginAdapter(void* h, Plugin_Destroy_Fn destroyFn, Plugin_GetMetadataJson_Fn metaFn,
                              Plugin_Initialize_Fn initFn, Plugin_Shutdown_Fn shutdownFn,
                              Plugin_GetLastError_Fn lastErrFn)
                : handle_(h), destroyFn_(destroyFn), metaFn_(metaFn), initFn_(initFn), shutdownFn_(shutdownFn), lastErrFn_(lastErrFn)
            {
                try {
                    const char* js = metaFn_(handle_);
                    if (js) {
                        nlohmann::json j = nlohmann::json::parse(js);
                        PluginApi_FreeString(const_cast<char*>(js));
                        meta_.id = j.value("id", "");
                        meta_.name = j.value("name", "");
                        meta_.version = j.value("version", "");
                        meta_.apiVersion = j.value("apiVersion", "1.0.0");
                        meta_.author = j.value("author", "");
                        meta_.description = j.value("description", "");
                        meta_.website = j.value("website", "");
                        // Preserve plugin type from metadata if present
                        int t = j.value("type", static_cast<int>(PluginType::Custom));
                        meta_.type = static_cast<PluginType>(t);
                    }
                } catch (...) {}
            }
            ~CAbiPluginAdapter() override {
                if (destroyFn_ && handle_) destroyFn_(handle_);
            }
            PluginMetadata GetMetadata() const override { return meta_; }
            bool Initialize() override { return initFn_ ? initFn_(handle_) : true; }
            void Shutdown() override { if (shutdownFn_) shutdownFn_(handle_); }
            PluginStatus GetStatus() const override { return PluginStatus::Active; }
            std::string GetLastError() const override { if (lastErrFn_) { const char* s = lastErrFn_(handle_); if (s) { std::string r(s); PluginApi_FreeString(const_cast<char*>(s)); return r; } } return {}; }
            bool IsLicensed() const override { return true; }
            bool SetLicense(const std::string&) override { return true; }
            bool ValidateConfiguration() const override { return true; }
        private:
            void* handle_ = nullptr;
            Plugin_Destroy_Fn destroyFn_ = nullptr;
            Plugin_GetMetadataJson_Fn metaFn_ = nullptr;
            Plugin_Initialize_Fn initFn_ = nullptr;
            Plugin_Shutdown_Fn shutdownFn_ = nullptr;
            Plugin_GetLastError_Fn lastErrFn_ = nullptr;
            PluginMetadata meta_;
        };

        auto adapter = std::make_unique<CAbiPluginAdapter>(pluginHandle, c_destroy, c_getMeta, c_initialize, c_shutdown, c_getLastErr);
        if (outLibraryHandle) *outLibraryHandle = handle;
        if (outPluginOpaqueHandle) *outPluginOpaqueHandle = pluginHandle;
        util::Logger::Info("PluginManager: Created C-ABI plugin adapter (unix)");
        return util::Result<std::unique_ptr<IPlugin>, error::Error>::Ok(std::move(adapter));
    }
#endif

    // All plugin creation paths handled above (C-ABI adapter). If we reach
    // here it means an unexpected path was taken; reject the plugin.
    UnloadLibrary(handle);
    return util::Result<std::unique_ptr<IPlugin>, error::Error>::Err(
        error::Error(error::ErrorCode::RuntimeError, "Plugin creation path not supported (legacy CreatePlugin removed)"));
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
    // Destroy any plugin-created AI service before shutdown
    if (info.pluginDestroyAIService && info.pluginServiceHandle) {
        using DestroyServiceFn = void(*)(void*);
        auto fn = reinterpret_cast<DestroyServiceFn>(info.pluginDestroyAIService);
        try { fn(info.pluginServiceHandle); } catch(...) {}
        info.pluginServiceHandle = nullptr;
        util::Logger::Info("PluginManager: Destroyed plugin AI service for {} during unload", pluginId);
    }
    if (info.instance)
    {
        // Shutdown and destroy the plugin instance before unloading the
        // library so any C-ABI destroy callbacks remain valid while
        // being invoked from the adapter destructor.
        info.instance->Shutdown();
        info.instance.reset();
    }
    if (info.libraryHandle)
    {
        UnloadLibrary(info.libraryHandle);
        info.libraryHandle = nullptr;
    }

    // Notify observers before unloading
    NotifyObservers(PluginEvent::Unloaded, pluginId, nullptr);

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

#if defined(_WIN32) || defined(__linux__)
    // On Windows/Linux, only ZIP archives are accepted for manual installation
    if (sourcePath.extension() != ".zip")
    {
        return util::Result<std::string, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument,
                "Only zipped plugins are supported. Expected .zip: " + sourcePath.string()));
    }
#else
    // On macOS, also accept plugin directories (used by bundled plugins)
    if (sourcePath.extension() != ".zip" && !std::filesystem::is_directory(sourcePath))
    {
        return util::Result<std::string, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument,
                "Only zipped plugins or plugin directories are supported. Expected .zip or directory: " + sourcePath.string()));
    }
#endif

    try {
        // Ensure plugins directory exists
        if (!std::filesystem::exists(m_pluginsDirectory)) {
            std::filesystem::create_directories(m_pluginsDirectory);
        }

        // Prepare to register plugin
        std::string pluginId;

        // If the source is a ZIP archive, extract it directly into a plugin
        // working directory under the user's plugins folder. We do NOT copy
        // the archive itself into the working dir to avoid unnecessary files.
        if (sourcePath.extension() == ".zip") {
            std::filesystem::path targetDir = m_pluginsDirectory / sourcePath.stem();
            // Remove any stale content before extracting
            if (std::filesystem::exists(targetDir)) {
                std::filesystem::remove_all(targetDir);
            }
            std::filesystem::create_directories(targetDir);

            if (!ExtractZipPlugin(sourcePath, targetDir)) {
                return util::Result<std::string, error::Error>::Err(
                    error::Error(error::ErrorCode::RuntimeError,
                        "Failed to extract plugin archive: " + sourcePath.string()));
            }

            util::Logger::Info("PluginManager: Extracted plugin {} to {}",
                sourcePath.string(), targetDir.string());

                // Load from the extracted directory
                auto loadResult = LoadPlugin(targetDir);
                if (loadResult.isErr()) {
                    // Clean up extracted folder if load failed
                    try { std::filesystem::remove_all(targetDir); } catch (...) {}
                    return util::Result<std::string, error::Error>::Err(loadResult.error());
                }

                pluginId = loadResult.unwrap();
                m_registeredPlugins[pluginId] = targetDir;
        } else {
            // Non-zip source: assume it's already a prepared plugin directory
            if (!std::filesystem::is_directory(sourcePath)) {
                return util::Result<std::string, error::Error>::Err(
                    error::Error(error::ErrorCode::InvalidArgument,
                        "Expected a zipped plugin or a prepared plugin directory: " + sourcePath.string()));
            }

            auto loadResult = LoadPlugin(sourcePath);
            if (loadResult.isErr()) {
                return util::Result<std::string, error::Error>::Err(loadResult.error());
            }

            pluginId = loadResult.unwrap();
            m_registeredPlugins[pluginId] = sourcePath;
        }
        
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
    std::filesystem::path pluginPackagePath;
    
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
    
    // Remove from registered plugins and delete deployed plugin folder/file
    auto it = m_registeredPlugins.find(pluginId);
    if (it != m_registeredPlugins.end()) {
        wasRegistered = true;
        std::filesystem::path pluginPath = it->second;
        pluginPackagePath = pluginPath;

        try {
            if (std::filesystem::exists(pluginPath)) {
                if (std::filesystem::is_directory(pluginPath)) {
                    std::filesystem::remove_all(pluginPath);
                    util::Logger::Info("PluginManager: Removed plugin directory: {}", pluginPath.string());
                } else {
                    std::filesystem::remove(pluginPath);
                    util::Logger::Info("PluginManager: Removed plugin file: {}", pluginPath.string());
                }
            }
            m_registeredPlugins.erase(it);
        } catch (const std::exception& ex) {
            util::Logger::Error("PluginManager: Failed to remove plugin deployment: {}", ex.what());
            // Continue anyway - we'll still remove from registry and notify
        }
    }

    auto itPlugin = m_plugins.find(pluginId);
    if (itPlugin != m_plugins.end()) {
        m_plugins.erase(itPlugin);
    }

    // Drop cached configuration so it is not re-applied to a future plugin with the same id
    m_configCache.erase(pluginId);
    
    // If the plugin was neither loaded nor registered, return error
    if (!wasLoaded && !wasRegistered) {
        return util::Result<bool, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument,
                "Plugin not found: " + pluginId));
    }
    
    // Clean extracted payload if present. If the registered path was a
    // directory we already removed it above; if it was a file we remove
    // the corresponding extracted folder (parent/stem) if present.
    if (!pluginPackagePath.empty()) {
        try {
            if (std::filesystem::exists(pluginPackagePath)) {
                // Already removed above when handling registered entry
            } else {
                // If pluginPackagePath pointed to a file originally, remove its extract dir
                auto extractDir = pluginPackagePath.parent_path() / pluginPackagePath.stem();
                if (std::filesystem::exists(extractDir)) {
                    std::filesystem::remove_all(extractDir);
                    util::Logger::Info("PluginManager: Removed extracted plugin folder: {}", extractDir.string());
                }
            }
        } catch (const std::exception& ex) {
            util::Logger::Warn("PluginManager: Failed to remove extracted plugin folder: {}", ex.what());
        }
    }

    // Persist removal so the plugin does not reappear on next launch
    auto saveResult = SaveConfiguration();
    if (saveResult.isErr()) {
        util::Logger::Warn("PluginManager: Failed to save configuration after unregister: {}", saveResult.error().what());
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

        // Events access is provided via PluginEvents_Register; no AI-specific
        // SetAIEvents call is performed here. Plugins should use PluginEvents_* helpers.
        // If the plugin exposes a C-ABI setter for the opaque events container,
        // call it now so SDK-first plugins can access events via the provided
        // opaque pointer.
        if (info.pluginOpaqueHandle && info.pluginSetAIEventsContainer && m_eventsContainerOpaque) {
            using SetEventsFn = void(*)(void*, void*);
            auto fn = reinterpret_cast<SetEventsFn>(info.pluginSetAIEventsContainer);
            try { fn(info.pluginOpaqueHandle, m_eventsContainerOpaque); } catch(...) {}
        }

        if (info.pluginOpaqueHandle && info.pluginSetEventsCallbacks && m_eventsContainerOpaque && m_eventsGetSizeFn && m_eventsGetEventJsonFn) {
            using SetCallbacksFn = void(*)(void*, void*, EventsContainer_GetSize_Fn, EventsContainer_GetEventJson_Fn);
            auto fn = reinterpret_cast<SetCallbacksFn>(info.pluginSetEventsCallbacks);
            try { fn(info.pluginOpaqueHandle, m_eventsContainerOpaque, m_eventsGetSizeFn, m_eventsGetEventJsonFn); } catch(...) {}
        }

        if (info.pluginOpaqueHandle && info.pluginSetHostUiCallbacks && m_hostUiOpaque && m_hostUiCallbacks.size >= sizeof(PluginHostUiCallbacks)) {
            using SetHostUiCallbacksFn = void(*)(void*, void*, const PluginHostUiCallbacks*);
            auto fn = reinterpret_cast<SetHostUiCallbacksFn>(info.pluginSetHostUiCallbacks);
            util::Logger::Debug("PluginManager: Injecting host UI callbacks into {}", pluginId);
            try { fn(info.pluginOpaqueHandle, m_hostUiOpaque, &m_hostUiCallbacks); } catch(...) {}
        }

        // If the plugin provides an AI service factory, create a service instance
        // so plugin panels relying on an internal service have it available.
        if (info.pluginCreateAIService && !info.pluginServiceHandle && info.pluginOpaqueHandle) {
            using CreateServiceFn = void*(*)(void*, const char*);
            auto fn = reinterpret_cast<CreateServiceFn>(info.pluginCreateAIService);
            try {
                void* svc = fn(info.pluginOpaqueHandle, nullptr);
                if (svc) {
                    info.pluginServiceHandle = svc;
                    util::Logger::Info("PluginManager: Created plugin AI service for {}", pluginId);
                }
            } catch(...) {}
        }
        
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

    // Events access is provided via PluginEvents_Register; plugins should use
    // PluginEvents_GetSize / PluginEvents_GetEventJson helpers to read events.
    // If plugin exposes C-ABI setter for events, call it so plugin receives
    // the opaque events container from the application.
    if (info.pluginOpaqueHandle && info.pluginSetAIEventsContainer && m_eventsContainerOpaque) {
        using SetEventsFn = void(*)(void*, void*);
        auto fn = reinterpret_cast<SetEventsFn>(info.pluginSetAIEventsContainer);
        try { fn(info.pluginOpaqueHandle, m_eventsContainerOpaque); } catch(...) {}
    }

    if (info.pluginOpaqueHandle && info.pluginSetEventsCallbacks && m_eventsContainerOpaque && m_eventsGetSizeFn && m_eventsGetEventJsonFn) {
        using SetCallbacksFn = void(*)(void*, void*, EventsContainer_GetSize_Fn, EventsContainer_GetEventJson_Fn);
        auto fn = reinterpret_cast<SetCallbacksFn>(info.pluginSetEventsCallbacks);
        try { fn(info.pluginOpaqueHandle, m_eventsContainerOpaque, m_eventsGetSizeFn, m_eventsGetEventJsonFn); } catch(...) {}
    }

    if (info.pluginOpaqueHandle && info.pluginSetHostUiCallbacks && m_hostUiOpaque && m_hostUiCallbacks.size >= sizeof(PluginHostUiCallbacks)) {
        using SetHostUiCallbacksFn = void(*)(void*, void*, const PluginHostUiCallbacks*);
        auto fn = reinterpret_cast<SetHostUiCallbacksFn>(info.pluginSetHostUiCallbacks);
        util::Logger::Debug("PluginManager: Injecting host UI callbacks into {}", pluginId);
        try { fn(info.pluginOpaqueHandle, m_hostUiOpaque, &m_hostUiCallbacks); } catch(...) {}
    }
    // If the plugin provides an AI service factory, create a service instance
    if (info.pluginCreateAIService && !info.pluginServiceHandle && info.pluginOpaqueHandle) {
        using CreateServiceFn = void*(*)(void*, const char*);
        auto fn = reinterpret_cast<CreateServiceFn>(info.pluginCreateAIService);
        try {
            void* svc = fn(info.pluginOpaqueHandle, nullptr);
            if (svc) {
                info.pluginServiceHandle = svc;
                util::Logger::Info("PluginManager: Created plugin AI service for {}", pluginId);
            }
        } catch(...) {}
    }
    
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
    // If plugin provided an AI service instance, destroy it when disabled
    if (it->second.pluginDestroyAIService && it->second.pluginServiceHandle) {
        using DestroyServiceFn = void(*)(void*);
        auto fn = reinterpret_cast<DestroyServiceFn>(it->second.pluginDestroyAIService);
        try { fn(it->second.pluginServiceHandle); } catch(...) {}
        it->second.pluginServiceHandle = nullptr;
        util::Logger::Info("PluginManager: Destroyed plugin AI service for {}", pluginId);
    }
    
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
    auto it = std::ranges::find(m_observers, observer);
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

std::filesystem::path PluginManager::GetPluginConfigDirectory(const std::string& pluginId) const
{
    // Return path: ~/Library/Application Support/LogViewer/plugin_configs/<plugin_id>/
    // Use the app config directory, not the plugins binary directory
    auto configDir = config::GetConfig().GetDefaultAppPath() / "plugin_configs" / pluginId;
    return configDir;
}

std::filesystem::path PluginManager::GetPluginConfigPath(const std::string& pluginId) const
{
    // Return path: ~/Library/Application Support/LogViewer/plugin_configs/<plugin_id>/config.json
    return GetPluginConfigDirectory(pluginId) / "config.json";
}

} // namespace plugin
