#include "AIProviderPlugin.hpp"

#include "PluginC.h"
#include "PluginEventsC.h"
#include "PluginKeyEncryptionC.h"
#include "PluginLoggerC.h"
#include <fmt/format.h>

#include <fstream>
#include <filesystem>

#include <QCoreApplication>

namespace plugin
{

AIProviderPlugin::AIProviderPlugin()
{
    m_aiService = std::make_shared<ai::AIServiceWrapper>();
}

PluginMetadata AIProviderPlugin::GetMetadata() const
{
    return PluginMetadata{
        .id = "ai_provider",
        .name = "AI Provider",
        .version = "1.0.0",
        .apiVersion = "1.0.0",
        .author = "LogViewer Team",
        .description = "Built-in AI provider with analysis, config, and chat panels",
        .website = "https://github.com/",
        .type = PluginType::AIProvider,
        .requiresLicense = false,
        .dependencies = {}
    };
}

bool AIProviderPlugin::Initialize()
{
    m_status = PluginStatus::Initialized;
    return true;
}

void AIProviderPlugin::Shutdown()
{
    m_status = PluginStatus::Unloaded;
    m_analyzer.reset();
    m_aiService.reset();
    m_events.reset();
    m_analysisPanel = nullptr;
    m_configPanel = nullptr;
}

PluginStatus AIProviderPlugin::GetStatus() const
{
    return m_status;
}

std::string AIProviderPlugin::GetLastError() const
{
    return m_lastError;
}

bool AIProviderPlugin::IsLicensed() const
{
    return true;
}

bool AIProviderPlugin::SetLicense(const std::string&)
{
    return true;
}

bool AIProviderPlugin::ValidateConfiguration() const
{
    return true;
}

std::shared_ptr<ai::IAIService> AIProviderPlugin::CreateService(const nlohmann::json& settings)
{
    // Load plugin's own config file and apply to runtime config
    nlohmann::json pluginConfig = LoadPluginConfig();
    
    // Use plugin-local config as defaults; explicit settings override them
    const std::string provider = settings.value("provider", pluginConfig.value("provider", "ollama"));
    std::string apiKey = settings.value("apiKey", pluginConfig.value("apiKey", ""));
    // Decrypt via plugin API (application may register real decrypt function)
    if (!apiKey.empty()) {
        char* dec = PluginKeyEncryption_Decrypt(apiKey.c_str());
        if (dec) {
            apiKey = std::string(dec);
            PluginKeyEncryption_FreeString(dec);
        }
    }
    const std::string baseUrl = settings.value("baseUrl", pluginConfig.value("baseUrl", "http://localhost:11434"));
    const std::string model = settings.value("model", pluginConfig.value("model", "qwen2.5-coder:7b"));

    try
    {
        auto service = ai::AIServiceFactory::CreateClient(provider, apiKey, baseUrl, model);
        m_aiService->service = service;
        ensureAnalyzer();
        m_status = PluginStatus::Active;
        return service;
    }
    catch (const std::exception& ex)
    {
        m_lastError = ex.what();
        m_status = PluginStatus::Error;
        std::string msg = fmt::format("[AIProviderPlugin] Failed to create service: {}", ex.what());
        PluginLogger_Log(PLUGIN_LOG_ERROR, msg.c_str());
        return nullptr;
    }
}

void AIProviderPlugin::SetEventsContainer(std::shared_ptr<void> eventsContainer)
{
    m_events = std::move(eventsContainer);
    ensureAnalyzer();
}

void AIProviderPlugin::SetEventsContainerRaw(void* eventsContainerRaw)
{
    if (!eventsContainerRaw) {
        m_events.reset();
        return;
    }
    // Create a non-owning shared_ptr that will not free the underlying pointer
    m_events = std::shared_ptr<void>(eventsContainerRaw, [](void*){});
    ensureAnalyzer();
}

void AIProviderPlugin::OnEventsUpdated()
{
    // No-op for now; UI pulls data on demand
}

QWidget* AIProviderPlugin::CreateTab(QWidget* parent)
{
    std::string msg = fmt::format("CreateTab called: parent={:p}, m_aiService={}", 
                     static_cast<const void*>(parent), m_aiService != nullptr);
    PluginLogger_Log(PLUGIN_LOG_INFO, msg.c_str());
    ensureAnalyzer();
    if (!m_analyzer)
    {
        PluginLogger_Log(PLUGIN_LOG_WARN, "CreateTab: m_analyzer is null, returning nullptr");
        return nullptr;
    }

    m_analysisPanel = new ui::qt::AIAnalysisPanel(m_aiService, m_analyzer, nullptr, parent);
    msg = fmt::format("CreateTab: successfully created AIAnalysisPanel={:p}", static_cast<const void*>(m_analysisPanel));
    PluginLogger_Log(PLUGIN_LOG_INFO, msg.c_str());
    if (m_analysisPanel && m_configPanel)
    {
        m_analysisPanel->SetConfigPanel(m_configPanel);
    }
    return m_analysisPanel;
}

QWidget* AIProviderPlugin::GetConfigurationUI()
{
    // Legacy override without parent - call the new version with nullptr parent
    return GetConfigurationUI(nullptr);
}

QWidget* AIProviderPlugin::GetConfigurationUI(QWidget* parent)
{
    std::string msg = fmt::format("GetConfigurationUI called: parent={:p}, m_aiService={}, m_analyzer={}",
                     static_cast<const void*>(parent), m_aiService != nullptr, m_analyzer != nullptr);
    PluginLogger_Log(PLUGIN_LOG_INFO, msg.c_str());
    
    // Ensure service and analyzer are created
    if (!m_aiService) {
        PluginLogger_Log(PLUGIN_LOG_WARN, "GetConfigurationUI: m_aiService is null - service should be created before calling GetConfigurationUI");
        // Create a default service as fallback
        nlohmann::json emptySettings;
        CreateService(emptySettings);
    }
    
    if (!m_aiService || !m_aiService->service) {
        msg = fmt::format("GetConfigurationUI: Cannot create config UI - m_aiService={}, service={}",
                         m_aiService != nullptr, (m_aiService && m_aiService->service) != false);
        PluginLogger_Log(PLUGIN_LOG_ERROR, msg.c_str());
        return nullptr;
    }
    
    PluginLogger_Log(PLUGIN_LOG_INFO, "GetConfigurationUI: Ensuring analyzer is created");
    ensureAnalyzer();
    
    if (!m_analyzer) {
        PluginLogger_Log(PLUGIN_LOG_ERROR, "GetConfigurationUI: m_analyzer is null after ensureAnalyzer, cannot create config panel");
        return nullptr;
    }

    PluginLogger_Log(PLUGIN_LOG_INFO, "GetConfigurationUI: Creating AIConfigPanel with parent");
    m_configPanel = new ui::qt::AIConfigPanel(m_aiService, m_analyzer, parent);
    
    PluginLogger_Log(PLUGIN_LOG_INFO, fmt::format("GetConfigurationUI: Created AIConfigPanel={:p}", 
                     static_cast<const void*>(m_configPanel)).c_str());
    
    if (m_analysisPanel) {
        m_analysisPanel->SetConfigPanel(m_configPanel);
    }
    
    return m_configPanel;
}

QWidget* AIProviderPlugin::CreateBottomPanel(QWidget* parent, ai::IAIService*, void* events)
{
    std::string msg = fmt::format("CreateBottomPanel called: events={}, m_events={}, m_aiService={}, parent={:p}",
                     events != nullptr, m_events != nullptr, m_aiService != nullptr, static_cast<const void*>(parent));
    PluginLogger_Log(PLUGIN_LOG_INFO, msg.c_str());
    
    // Use stored events container if no events parameter provided (C-ABI path)
    void* eventsToUse = events ? events : m_events.get();
    
    if (!eventsToUse)
    {
        PluginLogger_Log(PLUGIN_LOG_WARN, "CreateBottomPanel: no events container available, returning nullptr");
        return nullptr;
    }

    if (!m_aiService || !m_aiService->service)
    {
        PluginLogger_Log(PLUGIN_LOG_WARN, "CreateBottomPanel: m_aiService or service is null, returning nullptr");
        return nullptr;
    }

    // Store opaque events container via SDK-managed setter; UI will use
    // PluginEvents_GetSize/PluginEvents_GetEventJson to access events.
    SetEventsContainerRaw(eventsToUse);
    auto* panel = new ui::qt::AIChatPanel(m_aiService, parent);
    msg = fmt::format("CreateBottomPanel: successfully created AIChatPanel={:p}", static_cast<const void*>(panel));
    PluginLogger_Log(PLUGIN_LOG_INFO, msg.c_str());
    return panel;
}

void AIProviderPlugin::ensureAnalyzer()
{
    std::string msg = fmt::format("ensureAnalyzer: m_aiService={}, m_aiService->service={}, m_analyzer={}",
                     m_aiService != nullptr, 
                     (m_aiService && m_aiService->service) != false,
                     m_analyzer != nullptr);
    PluginLogger_Log(PLUGIN_LOG_INFO, msg.c_str());
    
    if (!m_aiService || !m_aiService->service)
    {
        PluginLogger_Log(PLUGIN_LOG_WARN, "ensureAnalyzer: m_aiService or m_aiService->service is null, returning");
        return;
    }

    if (!m_analyzer)
    {
        PluginLogger_Log(PLUGIN_LOG_INFO, "ensureAnalyzer: creating LogAnalyzer");
        m_analyzer = std::make_shared<ai::LogAnalyzer>(m_aiService);
    }
}

std::filesystem::path AIProviderPlugin::GetConfigFilePath() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const std::string id = GetMetadata().id;
    return std::filesystem::path(appDir.toStdString()) / "plugins" / id / "config.json";
}

nlohmann::json AIProviderPlugin::LoadPluginConfig()
{
    std::filesystem::path configPath = GetConfigFilePath();
    
    if (!std::filesystem::exists(configPath))
    {
        {
            std::string m = fmt::format("[AIProviderPlugin] No config file found at {}, using defaults", configPath.string());
            PluginLogger_Log(PLUGIN_LOG_DEBUG, m.c_str());
        }
        return nlohmann::json::object();
    }

    try
    {
        std::ifstream file(configPath);
        if (!file.is_open())
        {
            std::string m = fmt::format("[AIProviderPlugin] Failed to open config file: {}", configPath.string());
            PluginLogger_Log(PLUGIN_LOG_WARN, m.c_str());
            return nlohmann::json::object();
        }

        nlohmann::json config;
        file >> config;
        {
            std::string m = fmt::format("[AIProviderPlugin] Loaded config from: {}", configPath.string());
            PluginLogger_Log(PLUGIN_LOG_INFO, m.c_str());
        }
        return config;
    }
    catch (const std::exception& ex)
    {
        {
            std::string m = fmt::format("[AIProviderPlugin] Failed to parse config file: {}", ex.what());
            PluginLogger_Log(PLUGIN_LOG_ERROR, m.c_str());
        }
        return nlohmann::json::object();
    }
}

void AIProviderPlugin::SavePluginConfig(const nlohmann::json& config)
{
    std::filesystem::path configPath = GetConfigFilePath();
    
    try
    {
        // Ensure directory exists
        std::filesystem::path configDir = configPath.parent_path();
        if (!std::filesystem::exists(configDir))
        {
            std::filesystem::create_directories(configDir);
        }

        std::ofstream file(configPath);
        if (!file.is_open())
        {
            std::string m = fmt::format("[AIProviderPlugin] Failed to open config file for writing: {}", configPath.string());
            PluginLogger_Log(PLUGIN_LOG_ERROR, m.c_str());
            return;
        }

        file << config.dump(2);
        file.close();
        {
            std::string m = fmt::format("[AIProviderPlugin] Saved config to: {}", configPath.string());
            PluginLogger_Log(PLUGIN_LOG_INFO, m.c_str());
        }
    }
    catch (const std::exception& ex)
    {
        {
            std::string m = fmt::format("[AIProviderPlugin] Failed to save config: {}", ex.what());
            PluginLogger_Log(PLUGIN_LOG_ERROR, m.c_str());
        }
    }
}

} // namespace plugin


extern "C" {

// Create/destroy plugin instance using opaque PluginHandle (defined in PluginC.h)
EXPORT_PLUGIN_SYMBOL PluginHandle Plugin_Create()
{
    try {
        auto* p = new plugin::AIProviderPlugin();
        return static_cast<PluginHandle>(p);
    } catch (...) { return nullptr; }
}

EXPORT_PLUGIN_SYMBOL void Plugin_SetLoggerCallback(PluginHandle h, PluginLogFn logFn)
{
    if (!h) return;
    // Set the global logger callback so all PluginLogger_Log calls work
    PluginLogger_Register(logFn);
}

EXPORT_PLUGIN_SYMBOL void Plugin_Destroy(PluginHandle h)
{
    if (!h) return;
    auto* p = reinterpret_cast<plugin::AIProviderPlugin*>(h);
    delete p;
}

// C ABI: create/destroy AI service wrappers and events container setter using PluginHandle
EXPORT_PLUGIN_SYMBOL PluginServiceHandle Plugin_CreateAIService(PluginHandle pluginHandle, const char* settingsJson)
{
    if (!pluginHandle) return nullptr;
    try {
        auto* plugin = reinterpret_cast<plugin::AIProviderPlugin*>(pluginHandle);
        nlohmann::json settings = nlohmann::json::object();
        if (settingsJson) {
            try { settings = nlohmann::json::parse(settingsJson); } catch (...) { settings = nlohmann::json::object(); }
        }
        auto svc = plugin->CreateService(settings);
        if (!svc) return nullptr;
        auto* wrapper = new std::shared_ptr<ai::IAIService>(svc);
        return static_cast<PluginServiceHandle>(wrapper);
    } catch (...) { return nullptr; }
}

EXPORT_PLUGIN_SYMBOL void Plugin_DestroyAIService(PluginServiceHandle svc)
{
    if (!svc) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<ai::IAIService>*>(svc);
    delete wrapper;
}

EXPORT_PLUGIN_SYMBOL void Plugin_SetAIEventsContainer(PluginHandle pluginHandle, void* eventsContainerOpaque)
{
    if (!pluginHandle) return;
    auto* plugin = reinterpret_cast<plugin::AIProviderPlugin*>(pluginHandle);
    plugin->SetEventsContainerRaw(eventsContainerOpaque);
}

EXPORT_PLUGIN_SYMBOL void Plugin_OnAIEventsUpdated(PluginHandle pluginHandle)
{
    if (!pluginHandle) return;
    auto* plugin = reinterpret_cast<plugin::AIProviderPlugin*>(pluginHandle);
    try { plugin->OnEventsUpdated(); } catch (...) {}
}

} // extern "C"

// Panel creation helpers exposed via C ABI so host can embed plugin widgets
extern "C" {

EXPORT_PLUGIN_SYMBOL void* Plugin_CreateMainPanel(PluginHandle pluginHandle, void* parentWidget, const char* /*settingsJson*/)
{
    if (!pluginHandle) return nullptr;
    auto* plugin = reinterpret_cast<plugin::AIProviderPlugin*>(pluginHandle);
    QWidget* parent = reinterpret_cast<QWidget*>(parentWidget);
    try {
        return static_cast<void*>(plugin->CreateTab(parent));
    } catch (...) { return nullptr; }
}

EXPORT_PLUGIN_SYMBOL void* Plugin_CreateBottomPanel(PluginHandle pluginHandle, void* parentWidget, const char* /*settingsJson*/)
{
    if (!pluginHandle) return nullptr;
    auto* plugin = reinterpret_cast<plugin::AIProviderPlugin*>(pluginHandle);
    QWidget* parent = reinterpret_cast<QWidget*>(parentWidget);
    try {
        // plugin expects events to be set via Plugin_SetAIEventsContainer; pass nullptr for service
        return static_cast<void*>(plugin->CreateBottomPanel(parent, nullptr, nullptr));
    } catch (...) { return nullptr; }
}

EXPORT_PLUGIN_SYMBOL void* Plugin_CreateLeftPanel(PluginHandle pluginHandle, void* parentWidget, const char* /*settingsJson*/)
{
    if (!pluginHandle) {
        PluginLogger_Log(PLUGIN_LOG_ERROR, "Plugin_CreateLeftPanel: pluginHandle is null");
        return nullptr;
    }
    
    auto* plugin = reinterpret_cast<plugin::AIProviderPlugin*>(pluginHandle);
    QWidget* parent = reinterpret_cast<QWidget*>(parentWidget);
    
    PluginLogger_Log(PLUGIN_LOG_INFO, fmt::format("Plugin_CreateLeftPanel called: parent={:p}",
                     static_cast<const void*>(parent)).c_str());
    
    try {
        QWidget* configWidget = plugin->GetConfigurationUI(parent);
        if (configWidget) {
            PluginLogger_Log(PLUGIN_LOG_INFO, fmt::format("Plugin_CreateLeftPanel: Created config widget={:p}",
                           static_cast<const void*>(configWidget)).c_str());
        } else {
            PluginLogger_Log(PLUGIN_LOG_WARN, "Plugin_CreateLeftPanel: GetConfigurationUI returned nullptr");
        }
        return static_cast<void*>(configWidget);
    } catch (const std::exception& e) {
        PluginLogger_Log(PLUGIN_LOG_ERROR, fmt::format("Plugin_CreateLeftPanel exception: {}", e.what()).c_str());
        return nullptr;
    } catch (...) {
        PluginLogger_Log(PLUGIN_LOG_ERROR, "Plugin_CreateLeftPanel: Unknown exception");
        return nullptr;
    }
}

} // extern "C"

// Additional required C-ABI helpers: metadata and last-error getters.
extern "C" {

EXPORT_PLUGIN_SYMBOL const char* Plugin_GetMetadataJson(PluginHandle pluginHandle)
{
    try {
        if (!pluginHandle) return nullptr;
        auto* plugin = reinterpret_cast<plugin::AIProviderPlugin*>(pluginHandle);
        auto meta = plugin->GetMetadata();
        nlohmann::json j;
        j["id"] = meta.id;
        j["name"] = meta.name;
        j["version"] = meta.version;
        j["apiVersion"] = meta.apiVersion;
        j["author"] = meta.author;
        j["description"] = meta.description;
        j["website"] = meta.website;
        j["type"] = static_cast<int>(meta.type);
        // Serialize
        std::string s = j.dump();
        char* out = static_cast<char*>(std::malloc(s.size() + 1));
        if (!out) return nullptr;
        memcpy(out, s.c_str(), s.size() + 1);
        return out;
    } catch (...) { return nullptr; }
}

EXPORT_PLUGIN_SYMBOL const char* Plugin_GetLastError(PluginHandle pluginHandle)
{
    try {
        if (!pluginHandle) return nullptr;
        auto* plugin = reinterpret_cast<plugin::AIProviderPlugin*>(pluginHandle);
        std::string e = plugin->GetLastError();
        if (e.empty()) return nullptr;
        char* out = static_cast<char*>(std::malloc(e.size() + 1));
        if (!out) return nullptr;
        memcpy(out, e.c_str(), e.size() + 1);
        return out;
    } catch (...) { return nullptr; }
}

} // extern "C"
