// AI provider plugin that supplies analysis, config, and chat panels via the plugin interfaces
#pragma once

#include "plugins/IPlugin.hpp"
#include "plugins/IAIPlugin.hpp"
#include "plugins/ai/AIAnalysisPanel.hpp"
#include "plugins/ai/AIConfigPanel.hpp"
#include "plugins/ai/AIChatPanel.hpp"
#include "plugins/ai/LogAnalyzer.hpp"
#include "plugins/ai/AIServiceFactory.hpp"
#include "config/Config.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>

namespace plugin
{

class AIProviderPlugin : public IPlugin, public IAIPlugin
{
public:
    AIProviderPlugin();
    ~AIProviderPlugin() override = default;

    // IPlugin
    PluginMetadata GetMetadata() const override;
    bool Initialize() override;
    void Shutdown() override;
    PluginStatus GetStatus() const override;
    std::string GetLastError() const override;
    bool IsLicensed() const override;
    bool SetLicense(const std::string& licenseKey) override;
    bool ValidateConfiguration() const override;
    QWidget* CreateTab(QWidget* parent) override;
    QWidget* CreateBottomPanel(QWidget* parent, ai::IAIService* service, db::EventsContainer* events) override;
    QWidget* GetConfigurationUI() override;
    IAIPlugin* GetAIPluginInterface() override { return this; }

    // IAIPlugin
    std::shared_ptr<ai::IAIService> CreateService(const nlohmann::json& settings) override;
    void SetEventsContainer(std::shared_ptr<db::EventsContainer> eventsContainer) override;
    void OnEventsUpdated() override;

private:
    void ensureAnalyzer();
    nlohmann::json LoadPluginConfig();
    void SavePluginConfig(const nlohmann::json& config);
    std::filesystem::path GetConfigFilePath() const;

    PluginStatus m_status {PluginStatus::Unloaded};
    std::string m_lastError;

    std::shared_ptr<ai::AIServiceWrapper> m_aiService;
    std::shared_ptr<ai::LogAnalyzer> m_analyzer;
    std::shared_ptr<db::EventsContainer> m_events;

    // UI instances managed by Qt parentage
    ui::qt::AIAnalysisPanel* m_analysisPanel {nullptr};
    ui::qt::AIConfigPanel* m_configPanel {nullptr};
};

} // namespace plugin
