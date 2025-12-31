#include "AIProviderPlugin.hpp"

#include "PluginC.h"
#include "PluginEventsC.h"
#include "PluginKeyEncryptionC.h"
#include "PluginLoggerC.h"
#include "PluginAIProviderC.h"
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
    config::GetConfig().ApplyJson(pluginConfig);

    // Use plugin-local config as defaults; explicit settings override them
    const auto& cfg = config::GetConfig();
    const std::string provider = settings.value("provider", pluginConfig.value("provider", cfg.aiProvider));
    std::string apiKey = settings.value("apiKey", pluginConfig.value("apiKey", cfg.openaiApiKey));
    // Decrypt via plugin API (application may register real decrypt function)
    if (!apiKey.empty()) {
        char* dec = PluginKeyEncryption_Decrypt(apiKey.c_str());
        if (dec) {
            apiKey = std::string(dec);
            PluginKeyEncryption_FreeString(dec);
        }
    }
    const std::string baseUrl = settings.value("baseUrl", pluginConfig.value("baseUrl", cfg.ollamaBaseUrl));
    const std::string model = settings.value("model", pluginConfig.value("model", cfg.ollamaDefaultModel));

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
    ensureAnalyzer();
    if (!m_analyzer)
        return nullptr;

    m_analysisPanel = new ui::qt::AIAnalysisPanel(m_aiService, m_analyzer, nullptr, parent);
    if (m_analysisPanel && m_configPanel)
    {
        m_analysisPanel->SetConfigPanel(m_configPanel);
    }
    return m_analysisPanel;
}

QWidget* AIProviderPlugin::GetConfigurationUI()
{
    // Ensure service and analyzer are created
    if (!m_aiService) {
        // Create a default service if not already created
        nlohmann::json emptySettings;
        CreateService(emptySettings);
    }
    
    ensureAnalyzer();
    if (!m_analyzer)
        return nullptr;

    m_configPanel = new ui::qt::AIConfigPanel(m_aiService, m_analyzer);
    if (m_analysisPanel)
    {
        m_analysisPanel->SetConfigPanel(m_configPanel);
    }
    return m_configPanel;
}

QWidget* AIProviderPlugin::CreateBottomPanel(QWidget* parent, ai::IAIService*, void* events)
{
    if (!events)
        return nullptr;

    if (!m_aiService || !m_aiService->service)
        return nullptr;

    // Store opaque events container via SDK-managed setter; UI will use
    // PluginEvents_GetSize/PluginEvents_GetEventJson to access events.
    SetEventsContainerRaw(events);
    return new ui::qt::AIChatPanel(m_aiService, parent);
}

void AIProviderPlugin::ensureAnalyzer()
{
    if (!m_aiService || !m_aiService->service)
        return;

    if (!m_analyzer)
    {
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

EXPORT_PLUGIN_SYMBOL void Plugin_Destroy(PluginHandle h)
{
    if (!h) return;
    auto* p = reinterpret_cast<plugin::AIProviderPlugin*>(h);
    delete p;
}

// C ABI: create/destroy AI service wrappers and events container setter using PluginHandle
EXPORT_PLUGIN_SYMBOL AIServiceHandle Plugin_CreateAIService(PluginHandle pluginHandle, const char* settingsJson)
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
        return static_cast<AIServiceHandle>(wrapper);
    } catch (...) { return nullptr; }
}

EXPORT_PLUGIN_SYMBOL void Plugin_DestroyAIService(AIServiceHandle svc)
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
