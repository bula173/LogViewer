#pragma once

#include "IPlugin.hpp"
#include "IAnalysisPlugin.hpp"
#include "EventsContainer.hpp"
#include "ConfigWidget.hpp"
#include "ResultsWidget.hpp"
#include "AnomalyChartWidget.hpp"
#include "MetricsEngine.hpp"

#include <QPointer>
#include <QTabWidget>
#include <memory>
#include <string>

namespace plugin
{

class LogAnomalyMetricsPlugin : public IPlugin, public IAnalysisPlugin
{
public:
    LogAnomalyMetricsPlugin();
    ~LogAnomalyMetricsPlugin() override = default;

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
    QWidget* CreateFilterTab(QWidget* parent) override;
    IAnalysisPlugin* GetAnalysisInterface() override { return this; }

    // IAnalysisPlugin
    void SetEventsContainer(std::shared_ptr<db::EventsContainer> eventsContainer) override;
    void OnEventsUpdated() override;
    std::string GetAnalysisName() const override;

private:
    void refreshResults();

    PluginStatus m_status;
    std::string m_lastError;
    std::shared_ptr<db::EventsContainer> m_events;
    std::unique_ptr<MetricsEngine> m_engine;
    QPointer<QTabWidget> m_tabWidget;
    QPointer<ResultsWidget> m_resultsWidget;
    QPointer<ConfigWidget> m_configWidget;
    QPointer<AnomalyChartWidget> m_chartWidget;
    std::vector<std::string> m_ruleNames;
};

} // namespace plugin
