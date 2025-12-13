#include "application/plugins/IPlugin.hpp"
#include "application/plugins/IAnalysisPlugin.hpp"
#include "application/db/EventsContainer.hpp"
#include "EventMetricsConfigWidget.hpp"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QTabWidget>
#include <QTextEdit>
#include <QPointer>
#include <QList>
#include <map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace plugin
{

struct AnomalyDetection {
    std::string type;
    std::string description;
    std::string severity;  // "Low", "Medium", "High", "Critical"
    int affectedCount;
    double score;  // Anomaly score 0-100
};

/**
 * @brief Widget that displays event metrics and anomaly detection
 */
class EventMetricsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EventMetricsWidget(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_updateTimer(new QTimer(this))
    {
        setupUI();
        
        // Auto-refresh every 5 seconds when data changes
        m_updateTimer->setInterval(5000);
        connect(m_updateTimer, &QTimer::timeout, this, &EventMetricsWidget::refreshMetrics);
    }

    void setEventsContainer(std::shared_ptr<db::EventsContainer> container)
    {
        m_eventsContainer = container;
        refreshMetrics();
    }

    void refreshMetrics()
    {
        if (!m_eventsContainer) {
            return;
        }

        // Clear previous data
        m_typeTable->setRowCount(0);
        m_anomalyTable->setRowCount(0);
        m_statsLabel->clear();
        m_anomalySummary->clear();

        size_t totalEvents = m_eventsContainer->Size();
        if (totalEvents == 0) {
            m_statsLabel->setText("No events to analyze");
            return;
        }

        // Calculate metrics
        std::map<std::string, int> typeFrequency;
        std::map<std::string, std::vector<int>> typeIndices;
        int errorCount = 0;
        int warningCount = 0;

        for (size_t i = 0; i < totalEvents; ++i) {
            const auto& event = m_eventsContainer->GetEvent(static_cast<int>(i));
            std::string type = event.findByKey("type");
            if (type.empty()) type = event.findByKey("level");
            if (type.empty()) type = event.findByKey("severity");
            if (type.empty()) type = "unknown";
            
            typeFrequency[type]++;
            typeIndices[type].push_back(static_cast<int>(i));
            
            // Count errors and warnings
            std::string typeLower = type;
            std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);
            if (typeLower.find("error") != std::string::npos || typeLower.find("fatal") != std::string::npos) {
                errorCount++;
            } else if (typeLower.find("warn") != std::string::npos) {
                warningCount++;
            }
        }

        // Detect anomalies
        std::vector<AnomalyDetection> anomalies = detectAnomalies(typeFrequency, typeIndices, totalEvents);

        // Sort by frequency (descending)
        std::vector<std::pair<std::string, int>> sortedTypes(
            typeFrequency.begin(), typeFrequency.end()
        );
        std::sort(sortedTypes.begin(), sortedTypes.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        // Populate frequency table
        m_typeTable->setRowCount(static_cast<int>(sortedTypes.size()));
        int row = 0;
        for (const auto& [type, count] : sortedTypes) {
            double percentage = (count * 100.0 / totalEvents);

            m_typeTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(type)));
            m_typeTable->setItem(row, 1, new QTableWidgetItem(QString::number(count)));
            m_typeTable->setItem(row, 2, new QTableWidgetItem(QString::number(percentage, 'f', 2) + "%"));
            row++;
        }

        // Populate anomaly table
        m_anomalyTable->setRowCount(static_cast<int>(anomalies.size()));
        row = 0;
        for (const auto& anomaly : anomalies) {
            auto* typeItem = new QTableWidgetItem(QString::fromStdString(anomaly.type));
            auto* descItem = new QTableWidgetItem(QString::fromStdString(anomaly.description));
            auto* severityItem = new QTableWidgetItem(QString::fromStdString(anomaly.severity));
            auto* scoreItem = new QTableWidgetItem(QString::number(anomaly.score, 'f', 1));
            
            // Color code severity
            QColor color;
            if (anomaly.severity == "Critical") color = QColor(255, 0, 0, 50);
            else if (anomaly.severity == "High") color = QColor(255, 140, 0, 50);
            else if (anomaly.severity == "Medium") color = QColor(255, 215, 0, 50);
            else color = QColor(173, 216, 230, 50);
            
            for (int col = 0; col < 4; ++col) {
                if (col == 0) m_anomalyTable->setItem(row, col, typeItem);
                else if (col == 1) m_anomalyTable->setItem(row, col, descItem);
                else if (col == 2) m_anomalyTable->setItem(row, col, severityItem);
                else m_anomalyTable->setItem(row, col, scoreItem);
                m_anomalyTable->item(row, col)->setBackground(color);
            }
            row++;
        }

        // Update statistics
        double errorRate = (totalEvents > 0) ? (errorCount * 100.0 / totalEvents) : 0.0;
        double warningRate = (totalEvents > 0) ? (warningCount * 100.0 / totalEvents) : 0.0;
        
        QString stats = QString("Total Events: %1 | Unique Types: %2 | Errors: %3 (%4%) | Warnings: %5 (%6%)")
            .arg(totalEvents)
            .arg(typeFrequency.size())
            .arg(errorCount).arg(errorRate, 0, 'f', 2)
            .arg(warningCount).arg(warningRate, 0, 'f', 2);
        m_statsLabel->setText(stats);
        
        // Update anomaly summary
        updateAnomalySummary(anomalies, errorRate, warningRate);
    }
    
    std::vector<AnomalyDetection> detectAnomalies(
        const std::map<std::string, int>& typeFrequency,
        const std::map<std::string, std::vector<int>>& typeIndices,
        size_t totalEvents)
    {
        std::vector<AnomalyDetection> anomalies;
        
        // Calculate mean and standard deviation for event frequencies
        double mean = totalEvents / static_cast<double>(typeFrequency.size());
        double variance = 0.0;
        for (const auto& [type, count] : typeFrequency) {
            variance += std::pow(count - mean, 2);
        }
        variance /= typeFrequency.size();
        double stdDev = std::sqrt(variance);
        
        // 1. Detect frequency anomalies (events that occur too often or too rarely)
        for (const auto& [type, count] : typeFrequency) {
            double zscore = (count - mean) / (stdDev + 1e-10);
            
            if (std::abs(zscore) > 2.5) {  // More than 2.5 standard deviations
                AnomalyDetection anomaly;
                anomaly.type = type;
                anomaly.affectedCount = count;
                anomaly.score = std::min(100.0, std::abs(zscore) * 20.0);
                
                if (zscore > 0) {
                    anomaly.description = QString("Unusually high frequency (%1x normal)")
                        .arg(zscore, 0, 'f', 1).toStdString();
                    anomaly.severity = zscore > 4.0 ? "Critical" : (zscore > 3.0 ? "High" : "Medium");
                } else {
                    anomaly.description = QString("Unusually low frequency (%1x normal)")
                        .arg(std::abs(zscore), 0, 'f', 1).toStdString();
                    anomaly.severity = "Low";
                }
                anomalies.push_back(anomaly);
            }
        }
        
        // 2. Detect burst patterns (consecutive events of same type)
        for (const auto& [type, indices] : typeIndices) {
            if (indices.size() < 5) continue;
            
            int maxBurst = 1;
            int currentBurst = 1;
            
            for (size_t i = 1; i < indices.size(); ++i) {
                if (indices[i] - indices[i-1] <= 2) {  // Events within 2 positions
                    currentBurst++;
                    maxBurst = std::max(maxBurst, currentBurst);
                } else {
                    currentBurst = 1;
                }
            }
            
            if (maxBurst >= 10) {  // 10 or more consecutive events
                AnomalyDetection anomaly;
                anomaly.type = type;
                anomaly.description = QString("Event burst detected (%1 consecutive occurrences)")
                    .arg(maxBurst).toStdString();
                anomaly.affectedCount = maxBurst;
                anomaly.score = std::min(100.0, maxBurst * 5.0);
                anomaly.severity = maxBurst > 50 ? "Critical" : (maxBurst > 25 ? "High" : "Medium");
                anomalies.push_back(anomaly);
            }
        }
        
        // 3. Detect error-specific anomalies
        for (const auto& [type, count] : typeFrequency) {
            std::string typeLower = type;
            std::transform(typeLower.begin(), typeLower.end(), typeLower.begin(), ::tolower);
            
            double percentage = (count * 100.0) / totalEvents;
            
            if ((typeLower.find("error") != std::string::npos || 
                 typeLower.find("fatal") != std::string::npos) && 
                percentage > 5.0) {
                AnomalyDetection anomaly;
                anomaly.type = type;
                anomaly.description = QString("High error rate detected (%1%)")
                    .arg(percentage, 0, 'f', 2).toStdString();
                anomaly.affectedCount = count;
                anomaly.score = std::min(100.0, percentage * 10.0);
                anomaly.severity = percentage > 20.0 ? "Critical" : (percentage > 10.0 ? "High" : "Medium");
                anomalies.push_back(anomaly);
            }
        }
        
        // Sort anomalies by score (descending)
        std::sort(anomalies.begin(), anomalies.end(),
            [](const AnomalyDetection& a, const AnomalyDetection& b) { 
                return a.score > b.score; 
            });
        
        return anomalies;
    }
    
    void updateAnomalySummary(const std::vector<AnomalyDetection>& anomalies, 
                             double errorRate, double warningRate)
    {
        std::ostringstream summary;
        summary << std::fixed << std::setprecision(2);
        
        summary << "<h3>Anomaly Detection Summary</h3>";
        
        if (anomalies.empty()) {
            summary << "<p style='color: green;'><b>✓ No significant anomalies detected</b></p>";
            summary << "<p>Log patterns appear normal within expected parameters.</p>";
        } else {
            int critical = 0, high = 0, medium = 0, low = 0;
            for (const auto& a : anomalies) {
                if (a.severity == "Critical") critical++;
                else if (a.severity == "High") high++;
                else if (a.severity == "Medium") medium++;
                else low++;
            }
            
            summary << "<p><b>⚠ " << anomalies.size() << " anomalies detected:</b></p>";
            summary << "<ul>";
            if (critical > 0) summary << "<li style='color: red;'><b>Critical:</b> " << critical << "</li>";
            if (high > 0) summary << "<li style='color: orange;'><b>High:</b> " << high << "</li>";
            if (medium > 0) summary << "<li style='color: #DAA520;'><b>Medium:</b> " << medium << "</li>";
            if (low > 0) summary << "<li style='color: gray;'><b>Low:</b> " << low << "</li>";
            summary << "</ul>";
            
            // Top 3 anomalies
            summary << "<h4>Top Concerns:</h4>";
            summary << "<ol>";
            for (size_t i = 0; i < std::min(size_t(3), anomalies.size()); ++i) {
                const auto& a = anomalies[i];
                summary << "<li><b>" << a.type << "</b>: " << a.description 
                       << " (Score: " << a.score << ")</li>";
            }
            summary << "</ol>";
        }
        
        // Health indicators
        summary << "<h4>System Health Indicators:</h4>";
        summary << "<ul>";
        
        if (errorRate > 10.0) {
            summary << "<li style='color: red;'>❌ <b>Error rate is high</b> (" << errorRate << "%)</li>";
        } else if (errorRate > 5.0) {
            summary << "<li style='color: orange;'>⚠ Error rate is elevated (" << errorRate << "%)</li>";
        } else if (errorRate > 0) {
            summary << "<li style='color: green;'>✓ Error rate is acceptable (" << errorRate << "%)</li>";
        }
        
        if (warningRate > 20.0) {
            summary << "<li style='color: orange;'>⚠ Warning rate is high (" << warningRate << "%)</li>";
        } else {
            summary << "<li style='color: green;'>✓ Warning rate is normal (" << warningRate << "%)</li>";
        }
        
        summary << "</ul>";
        
        m_anomalySummary->setHtml(QString::fromStdString(summary.str()));
    }

private slots:
    void onRefreshClicked()
    {
        refreshMetrics();
    }

    void onAutoRefreshToggled(bool checked)
    {
        if (checked) {
            m_updateTimer->start();
        } else {
            m_updateTimer->stop();
        }
    }

private:
    void setupUI()
    {
        auto* layout = new QVBoxLayout(this);

        // Title
        auto* titleLabel = new QLabel("<h2>Event Metrics</h2>");
        layout->addWidget(titleLabel);

        // Statistics label
        m_statsLabel = new QLabel();
        layout->addWidget(m_statsLabel);

        // Control buttons
        auto* buttonLayout = new QHBoxLayout();
        auto* refreshBtn = new QPushButton("Refresh Analysis");
        auto* autoRefreshBtn = new QPushButton("Auto-Refresh");
        autoRefreshBtn->setCheckable(true);
        
        connect(refreshBtn, &QPushButton::clicked, this, &EventMetricsWidget::onRefreshClicked);
        connect(autoRefreshBtn, &QPushButton::toggled, this, &EventMetricsWidget::onAutoRefreshToggled);
        
        buttonLayout->addWidget(refreshBtn);
        buttonLayout->addWidget(autoRefreshBtn);
        buttonLayout->addStretch();
        layout->addLayout(buttonLayout);

        // Tab widget for different views
        auto* tabWidget = new QTabWidget();
        
        // Anomaly Detection Tab
        auto* anomalyWidget = new QWidget();
        auto* anomalyLayout = new QVBoxLayout(anomalyWidget);
        
        m_anomalySummary = new QTextEdit();
        m_anomalySummary->setReadOnly(true);
        m_anomalySummary->setMaximumHeight(200);
        anomalyLayout->addWidget(m_anomalySummary);
        
        m_anomalyTable = new QTableWidget();
        m_anomalyTable->setColumnCount(4);
        m_anomalyTable->setHorizontalHeaderLabels({"Event Type", "Anomaly Description", "Severity", "Score"});
        m_anomalyTable->horizontalHeader()->setStretchLastSection(true);
        m_anomalyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_anomalyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_anomalyTable->setSortingEnabled(true);
        anomalyLayout->addWidget(m_anomalyTable);
        
        tabWidget->addTab(anomalyWidget, "🔍 Anomalies");
        
        // Frequency Distribution Tab
        auto* freqWidget = new QWidget();
        auto* freqLayout = new QVBoxLayout(freqWidget);
        
        m_typeTable = new QTableWidget();
        m_typeTable->setColumnCount(3);
        m_typeTable->setHorizontalHeaderLabels({"Event Type", "Count", "Percentage"});
        m_typeTable->horizontalHeader()->setStretchLastSection(true);
        m_typeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_typeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_typeTable->setSortingEnabled(true);
        freqLayout->addWidget(m_typeTable);
        
        tabWidget->addTab(freqWidget, "📊 Frequency Distribution");
        
        layout->addWidget(tabWidget);
        setLayout(layout);
    }

    std::shared_ptr<db::EventsContainer> m_eventsContainer;
    QTableWidget* m_typeTable;
    QTableWidget* m_anomalyTable;
    QTextEdit* m_anomalySummary;
    QLabel* m_statsLabel;
    QTimer* m_updateTimer;
};

/**
 * @brief Analysis plugin implementation for event metrics
 */
class EventMetricsAnalyzer : public IAnalysisPlugin
{
public:
    EventMetricsAnalyzer()
    {
    }

    void SetEventsContainer(std::shared_ptr<db::EventsContainer> eventsContainer) override
    {
        m_eventsContainer = eventsContainer;
        // Notify all existing widgets
        for (const auto& widgetPtr : m_widgets) {
            if (widgetPtr) {
                widgetPtr->setEventsContainer(eventsContainer);
            }
        }
    }

    void OnEventsUpdated() override
    {
        // Refresh all existing widgets
        for (const auto& widgetPtr : m_widgets) {
            if (widgetPtr) {
                widgetPtr->refreshMetrics();
            }
        }
    }

    std::string GetAnalysisName() const override
    {
        return "Event Metrics";
    }

    QWidget* createWidget(QWidget* parent)
    {
        // Always create a new widget
        auto* widget = new EventMetricsWidget(parent);
        if (m_eventsContainer) {
            widget->setEventsContainer(m_eventsContainer);
        }
        
        // Track widget with QPointer (auto-nulls when widget is deleted)
        m_widgets.append(QPointer<EventMetricsWidget>(widget));
        
        return widget;
    }

private:
    std::shared_ptr<db::EventsContainer> m_eventsContainer;
    QList<QPointer<EventMetricsWidget>> m_widgets;  // Track all created widgets
};

/**
 * @brief Event Metrics Plugin - provides frequency analysis of log events
 */
class EventMetricsPlugin : public IPlugin
{
public:
    EventMetricsPlugin()
        : m_status(PluginStatus::Unloaded)
        , m_analyzer(std::make_unique<EventMetricsAnalyzer>())
    {
    }

    PluginMetadata GetMetadata() const override
    {
        return PluginMetadata{
            "event_metrics_analyzer",
            "Event Metrics & Anomaly Detector",
            "2.0.0",
            "LogViewer Team",
            "Analyzes event patterns and detects anomalies using statistical analysis",
            "",
            PluginType::Analyzer,
            false,
            {}
        };
    }

    bool Initialize() override
    {
        try {
            m_status = PluginStatus::Initialized;
            return true;
        }
        catch (const std::exception& ex) {
            m_lastError = std::string("Initialization failed: ") + ex.what();
            m_status = PluginStatus::Error;
            return false;
        }
    }

    void Shutdown() override
    {
        m_status = PluginStatus::Unloaded;
    }

    PluginStatus GetStatus() const override
    {
        return m_status;
    }

    std::string GetLastError() const override
    {
        return m_lastError;
    }

    bool IsLicensed() const override
    {
        return true;
    }

    bool SetLicense(const std::string& /*licenseKey*/) override
    {
        return true;
    }

    IAnalysisPlugin* GetAnalysisInterface() override
    {
        return m_analyzer.get();
    }

    QWidget* CreateTab(QWidget* parent) override
    {
        return m_analyzer->createWidget(parent);
    }

    QWidget* CreateFilterTab(QWidget* parent) override
    {
        auto* configWidget = new EventMetricsConfigWidget(parent);
        
        // Connect configuration changes to analyzer refresh
        QObject::connect(configWidget, &EventMetricsConfigWidget::rulesChanged,
                        [this]() {
                            // Rules changed - could trigger metrics recalculation
                            // For now just log it
                        });
        
        return configWidget;
    }

    bool ValidateConfiguration() const override
    {
        return true;
    }

private:
    PluginStatus m_status;
    std::string m_lastError;
    std::unique_ptr<EventMetricsAnalyzer> m_analyzer;
};

} // namespace plugin

// Export plugin
EXPORT_PLUGIN(plugin::EventMetricsPlugin)

// Include MOC for Qt signals/slots
#include "EventMetricsPlugin.moc"
