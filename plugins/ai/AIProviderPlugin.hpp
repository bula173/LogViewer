// AI provider plugin that supplies analysis, config, and chat panels via the plugin interfaces
#pragma once

#include "AIAnalysisPanel.hpp"
#include "AIConfigPanel.hpp"
#include "AIChatPanel.hpp"
#include "LogAnalyzer.hpp"
#include "AIServiceFactory.hpp"
#include "Config.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>

#include "PluginTypesC.h"
#include <QWidget>

namespace plugin
{

// SDK-first plugin: do not depend on internal application C++ interfaces.
class AIProviderPlugin
{
public:
    AIProviderPlugin();
    ~AIProviderPlugin() = default;

    // Plugin API methods (no C++ interface inheritance in SDK-first mode)
    PluginMetadata GetMetadata() const;
    bool Initialize();
    void Shutdown();
    PluginStatus GetStatus() const;
    std::string GetLastError() const;
    bool IsLicensed() const;
    bool SetLicense(const std::string& licenseKey);
    bool ValidateConfiguration() const;
    QWidget* CreateTab(QWidget* parent);
    QWidget* CreateBottomPanel(QWidget* parent, ai::IAIService* service, void* events);
    QWidget* GetConfigurationUI();

    // IAIPlugin-like methods exposed for host use via C ABI
    std::shared_ptr<ai::IAIService> CreateService(const nlohmann::json& settings);
    void SetEventsContainer(std::shared_ptr<void> eventsContainer);
    // Non-owning setter used by C ABI bridge (host passes opaque raw pointer)
    void SetEventsContainerRaw(void* eventsContainerRaw);
    void OnEventsUpdated();

private:
    void ensureAnalyzer();
    nlohmann::json LoadPluginConfig();
    void SavePluginConfig(const nlohmann::json& config);
    std::filesystem::path GetConfigFilePath() const;

    PluginStatus m_status {PluginStatus::Unloaded};
    std::string m_lastError;

    std::shared_ptr<ai::AIServiceWrapper> m_aiService;
    std::shared_ptr<ai::LogAnalyzer> m_analyzer;
    std::shared_ptr<void> m_events;

    // UI instances managed by Qt parentage
    ui::qt::AIAnalysisPanel* m_analysisPanel {nullptr};
    ui::qt::AIConfigPanel* m_configPanel {nullptr};
};

} // namespace plugin
