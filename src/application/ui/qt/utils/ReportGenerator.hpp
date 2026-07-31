#pragma once

#include <QString>
#include <vector>
#include <nlohmann/json.hpp>

namespace db {
class EventsContainer;
class LogEvent;
}

namespace ui::qt::utils {

/**
 * @brief Report generation system for v1.10.0 Phase 11.
 *
 * Features:
 * - Generate comprehensive reports from log data
 * - Multiple report formats (HTML, PDF, Markdown, JSON)
 * - Customizable report sections (summary, statistics, events, analysis)
 * - Time range filtering
 * - Event severity breakdown
 * - Actor/source analysis
 * - Trends and patterns
 * - Export with styling and formatting
 */
class ReportGenerator
{
  public:
    enum class ReportFormat
    {
        HTML,
        PDF,
        Markdown,
        JSON,
        PlainText
    };

    struct ReportOptions
    {
        ReportFormat format {ReportFormat::HTML};
        bool includeSummary {true};
        bool includeStatistics {true};
        bool includeTimeline {true};
        bool includeTrends {true};
        bool includeEventList {true};
        bool includeActorAnalysis {true};
        int maxEventsInReport {1000};
        QString title {QT_TRANSLATE_NOOP("ReportGenerator", "Log Analysis Report")};
    };

    explicit ReportGenerator(db::EventsContainer& events);

    /// Generate a report from events in the container
    QString generateReport(const std::vector<int>& eventIndices, const ReportOptions& options);

    /// Generate from time range
    QString generateReportByTimeRange(int64_t startTime, int64_t endTime, const ReportOptions& options);

    /// Generate from filtered events (by level, pattern, etc.)
    QString generateReportFiltered(const QString& filterCriteria, const ReportOptions& options);

  private:
    struct ReportStatistics
    {
        int totalEvents;
        int criticalCount;
        int errorCount;
        int warningCount;
        int infoCount;
        int debugCount;
        int64_t timeSpanMs;
        std::vector<QString> uniqueActors;
        std::map<QString, int> levelDistribution;
    };

    QString GenerateHTML(const std::vector<const db::LogEvent*>& events, const ReportOptions& options);
    QString GenerateMarkdown(const std::vector<const db::LogEvent*>& events, const ReportOptions& options);
    QString GenerateJSON(const std::vector<const db::LogEvent*>& events, const ReportOptions& options);
    QString GeneratePlainText(const std::vector<const db::LogEvent*>& events, const ReportOptions& options);

    ReportStatistics CalculateStatistics(const std::vector<const db::LogEvent*>& events);
    QString GenerateSummary(const ReportStatistics& stats);
    QString GenerateStatisticsTable(const ReportStatistics& stats);
    QString GenerateTimeline(const std::vector<const db::LogEvent*>& events);
    QString GenerateTrendAnalysis(const std::vector<const db::LogEvent*>& events);

    db::EventsContainer& m_events;
};

} // namespace ui::qt::utils
