#include "plugins/ai/AIProviderPlugin.hpp"

#include "db/EventsContainer.hpp"
#include "plugins/PluginManager.hpp"
#include "util/Logger.hpp"

#include <fstream>
#include <filesystem>

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
    // Load plugin's own config file
    nlohmann::json pluginConfig = LoadPluginConfig();
    
    // Main config as ultimate fallback for defaults
    const auto& cfg = config::GetConfig();

    // Priority: provided settings > plugin config file > main config defaults
    const std::string provider = settings.value("provider", 
        pluginConfig.value("provider", cfg.aiProvider));
    const std::string apiKey = settings.value("apiKey", 
        pluginConfig.value("apiKey", cfg.GetApiKeyForProvider(provider)));
    const std::string baseUrl = settings.value("baseUrl", 
        pluginConfig.value("baseUrl", cfg.ollamaBaseUrl));
    const std::string model = settings.value("model", 
        pluginConfig.value("model", cfg.ollamaDefaultModel));

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
        util::Logger::Error("[AIProviderPlugin] Failed to create service: {}", ex.what());
        return nullptr;
    }
}

void AIProviderPlugin::SetEventsContainer(std::shared_ptr<db::EventsContainer> eventsContainer)
{
    m_events = std::move(eventsContainer);
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

QWidget* AIProviderPlugin::CreateBottomPanel(QWidget* parent, ai::IAIService*, db::EventsContainer* events)
{
    if (!events)
        return nullptr;

    if (!m_aiService || !m_aiService->service)
        return nullptr;

    return new ui::qt::AIChatPanel(m_aiService, *events, parent);
}

void AIProviderPlugin::ensureAnalyzer()
{
    if (!m_aiService || !m_aiService->service || !m_events)
        return;

    if (!m_analyzer)
    {
        m_analyzer = std::make_shared<ai::LogAnalyzer>(m_aiService, *m_events);
    }
}

std::filesystem::path AIProviderPlugin::GetConfigFilePath() const
{
    auto& pluginManager = PluginManager::GetInstance();
    return pluginManager.GetPluginConfigPath(GetMetadata().id);
}

nlohmann::json AIProviderPlugin::LoadPluginConfig()
{
    std::filesystem::path configPath = GetConfigFilePath();
    
    if (!std::filesystem::exists(configPath))
    {
        util::Logger::Debug("[AIProviderPlugin] No config file found at {}, using defaults", 
            configPath.string());
        return nlohmann::json::object();
    }

    try
    {
        std::ifstream file(configPath);
        if (!file.is_open())
        {
            util::Logger::Warn("[AIProviderPlugin] Failed to open config file: {}", 
                configPath.string());
            return nlohmann::json::object();
        }

        nlohmann::json config;
        file >> config;
        util::Logger::Info("[AIProviderPlugin] Loaded config from: {}", configPath.string());
        return config;
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[AIProviderPlugin] Failed to parse config file: {}", ex.what());
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
            util::Logger::Error("[AIProviderPlugin] Failed to open config file for writing: {}", 
                configPath.string());
            return;
        }

        file << config.dump(2);
        file.close();
        util::Logger::Info("[AIProviderPlugin] Saved config to: {}", configPath.string());
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[AIProviderPlugin] Failed to save config: {}", ex.what());
    }
}

} // namespace plugin

EXPORT_PLUGIN(plugin::AIProviderPlugin)
