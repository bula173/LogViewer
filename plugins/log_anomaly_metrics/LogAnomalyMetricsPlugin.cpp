#include "LogAnomalyMetricsPlugin.hpp"

#include "EventsContainer.hpp"

#include <QTabWidget>

#include <QVBoxLayout>
#include <QWidget>
#include <QObject>

namespace plugin
{

LogAnomalyMetricsPlugin::LogAnomalyMetricsPlugin()
    : m_status(PluginStatus::Unloaded)
    , m_engine(std::make_unique<MetricsEngine>())
{
}

PluginMetadata LogAnomalyMetricsPlugin::GetMetadata() const
{
    PluginMetadata meta;
    meta.id = "log_anomaly_metrics";
    meta.name = "Log Anomaly Metrics";
    meta.version = "0.1.0";
    meta.author = "LogViewer Team";
    meta.description = "Analyzer plugin for anomaly-oriented log metrics using Eigen and Boost";
    meta.website = "https://example.com/log-anomaly-metrics";
    meta.type = PluginType::Analyzer;
    meta.requiresLicense = false;
    return meta;
}

bool LogAnomalyMetricsPlugin::Initialize()
{
    m_status = PluginStatus::Initialized;
    return true;
}

void LogAnomalyMetricsPlugin::Shutdown()
{
    m_status = PluginStatus::Unloaded;
    m_resultsWidget.clear();
    m_configWidget.clear();
    m_events.reset();
}

PluginStatus LogAnomalyMetricsPlugin::GetStatus() const
{
    return m_status;
}

std::string LogAnomalyMetricsPlugin::GetLastError() const
{
    return m_lastError;
}

bool LogAnomalyMetricsPlugin::IsLicensed() const
{
    return true;
}

bool LogAnomalyMetricsPlugin::SetLicense(const std::string& /*licenseKey*/)
{
    return true;
}

bool LogAnomalyMetricsPlugin::ValidateConfiguration() const
{
    return true;
}

QWidget* LogAnomalyMetricsPlugin::CreateTab(QWidget* parent)
{
    if (!m_tabWidget)
    {
        m_tabWidget = new QTabWidget(parent);

        m_resultsWidget = new ResultsWidget(m_tabWidget);
        QObject::connect(m_resultsWidget, &ResultsWidget::EventActivated, m_resultsWidget,
            [this](int index) {
                if (m_events && index >= 0 && index < static_cast<int>(m_events->Size()))
                {
                    m_events->SetCurrentItem(index);
                    if (m_chartWidget)
                    {
                        m_chartWidget->SetHighlightIndex(index);
                    }
                }
            });
        m_tabWidget->addTab(m_resultsWidget, QObject::tr("Results"));

        m_chartWidget = new AnomalyChartWidget(m_tabWidget);
        m_tabWidget->addTab(m_chartWidget, QObject::tr("Chart"));
    }
    return m_tabWidget;
}

QWidget* LogAnomalyMetricsPlugin::CreateFilterTab(QWidget* parent)
{
    if (!m_configWidget)
    {
        m_configWidget = new ConfigWidget(parent);
        m_configWidget->SetEngine(m_engine.get());
        QObject::connect(m_configWidget, &ConfigWidget::configChanged, m_configWidget, [this]() { refreshResults(); });
    }
    return m_configWidget;
}

void LogAnomalyMetricsPlugin::SetEventsContainer(std::shared_ptr<db::EventsContainer> eventsContainer)
{
    m_events = std::move(eventsContainer);
    m_engine->SetEventsContainer(m_events);
    m_status = PluginStatus::Active;
    m_ruleNames = m_engine->GetRuleNames();
    refreshResults();
}

void LogAnomalyMetricsPlugin::OnEventsUpdated()
{
    refreshResults();
}

std::string LogAnomalyMetricsPlugin::GetAnalysisName() const
{
    return "Log Anomaly Metrics";
}

void LogAnomalyMetricsPlugin::refreshResults()
{
    if (!m_resultsWidget)
    {
        return;
    }

    const auto results = m_engine->Analyze();
    m_ruleNames = m_engine->GetRuleNames();
    m_resultsWidget->DisplayResults(results);
    if (m_chartWidget)
    {
        m_chartWidget->SetResults(results);
        m_chartWidget->SetRuleNames(m_ruleNames);
    }
}

} // namespace plugin

EXPORT_PLUGIN(plugin::LogAnomalyMetricsPlugin)
