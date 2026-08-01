#include "ReportGenerator.hpp"

#include "Logger.hpp"
#include "../../../db/EventsContainer.hpp"

#include <QDateTime>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace ui::qt::utils {

ReportGenerator::ReportGenerator(db::EventsContainer& events)
    : m_events(events)
{
}

QString ReportGenerator::generateReport(const std::vector<int>& eventIndices, const ReportOptions& options)
{
    std::vector<const db::LogEvent*> events;
    for (int idx : eventIndices)
    {
        if (idx >= 0 && idx < static_cast<int>(m_events.Size()))
            events.push_back(&m_events.GetEvent(static_cast<size_t>(idx)));
    }

    util::Logger::Info("[ReportGenerator] Generating report with {} events", events.size());

    switch (options.format)
    {
        case ReportFormat::HTML:
            return GenerateHTML(events, options);
        case ReportFormat::Markdown:
            return GenerateMarkdown(events, options);
        case ReportFormat::JSON:
            return GenerateJSON(events, options);
        case ReportFormat::PlainText:
            return GeneratePlainText(events, options);
        case ReportFormat::PDF:
            util::Logger::Warn("[ReportGenerator] PDF generation not yet implemented");
            return "PDF generation not implemented";
    }

    return {};
}

QString ReportGenerator::generateReportByTimeRange(int64_t startTime, int64_t endTime, const ReportOptions& options)
{
    // Note: Timestamp parsing from event data would be implemented based on format
    // For now, include all events
    std::vector<int> indices;
    for (size_t i = 0; i < m_events.Size(); ++i)
        indices.push_back(static_cast<int>(i));

    return generateReport(indices, options);
}

QString ReportGenerator::generateReportFiltered(const QString&, const ReportOptions& options)
{
    // This would integrate with existing filter system
    std::vector<int> indices;
    for (size_t i = 0; i < m_events.Size(); ++i)
        indices.push_back(static_cast<int>(i));

    return generateReport(indices, options);
}

QString ReportGenerator::GenerateHTML(const std::vector<const db::LogEvent*>& events, const ReportOptions& options)
{
    auto stats = CalculateStatistics(events);

    QString html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>)" + options.title + R"(</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; color: #333; }
        h1 { color: #0078D4; border-bottom: 2px solid #0078D4; padding-bottom: 10px; }
        h2 { color: #0078D4; margin-top: 30px; }
        table { border-collapse: collapse; width: 100%; margin: 15px 0; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; font-weight: bold; }
        .error { background-color: #ffcccc; }
        .warning { background-color: #fff3cd; }
        .info { background-color: #d1ecf1; }
        .summary { background-color: #f8f9fa; padding: 15px; border-radius: 5px; margin: 15px 0; }
        .stat-box { display: inline-block; margin: 10px 15px; }
    </style>
</head>
<body>
)";

    // Title
    html += "<h1>" + options.title + "</h1>\n";
    html += "<p>Generated: " + QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss") + "</p>\n";

    // Summary
    if (options.includeSummary)
    {
        html += "<div class=\"summary\">\n";
        html += GenerateSummary(stats);
        html += "</div>\n";
    }

    // Statistics
    if (options.includeStatistics)
    {
        html += "<h2>Statistics</h2>\n";
        html += GenerateStatisticsTable(stats);
    }

    // Event List
    if (options.includeEventList)
    {
        html += "<h2>Log Events</h2>\n";
        html += "<table>\n<tr><th>Timestamp</th><th>Level</th><th>Message</th><th>Actor</th></tr>\n";

        int count = 0;
        for (const auto* event : events)
        {
            if (count++ >= options.maxEventsInReport)
                break;

            QString level = QString::fromStdString(event->findByKey("level"));
            QString rowClass = (level == "ERROR" || level == "CRITICAL") ? " class=\"error\"" :
                              (level == "WARNING") ? " class=\"warning\"" : " class=\"info\"";

            // Escape all user-provided data to prevent XSS attacks
            auto escapeHtml = [](QString text) -> QString {
                return text
                    .replace("&", "&amp;")
                    .replace("<", "&lt;")
                    .replace(">", "&gt;")
                    .replace("\"", "&quot;")
                    .replace("'", "&#39;");
            };

            QString timestamp = escapeHtml(QString::fromStdString(event->findByKey("timestamp")));
            QString message = escapeHtml(QString::fromStdString(event->findByKey("message")).left(100));
            QString actor = escapeHtml(QString::fromStdString(event->findByKey("actor")));

            html += "<tr" + rowClass + ">";
            html += "<td>" + timestamp + "</td>";
            html += "<td>" + level + "</td>";
            html += "<td>" + message + "</td>";
            html += "<td>" + actor + "</td>";
            html += "</tr>\n";
        }
        html += "</table>\n";
    }

    html += "</body>\n</html>";
    return html;
}

QString ReportGenerator::GenerateMarkdown(const std::vector<const db::LogEvent*>& events, const ReportOptions& options)
{
    auto stats = CalculateStatistics(events);

    QString md = "# " + options.title + "\n\n";
    md += "**Generated**: " + QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss") + "\n\n";

    if (options.includeSummary)
    {
        md += "## Summary\n";
        md += GenerateSummary(stats);
        md += "\n";
    }

    if (options.includeStatistics)
    {
        md += "## Statistics\n";
        md += GenerateStatisticsTable(stats);
        md += "\n";
    }

    if (options.includeEventList)
    {
        md += "## Log Events\n\n";
        md += "| Timestamp | Level | Message | Actor |\n";
        md += "|-----------|-------|---------|-------|\n";

        int count = 0;
        for (const auto* event : events)
        {
            if (count++ >= options.maxEventsInReport)
                break;

            QString level = QString::fromStdString(event->findByKey("level"));
            QString timestamp = QString::fromStdString(event->findByKey("timestamp"));

            // Properly escape Markdown table cell content
            auto escapeMarkdownCell = [](QString text) -> QString {
                return text
                    .replace("\\", "\\\\")      // Backslash first!
                    .replace("|", "\\|")        // Pipe character
                    .replace("\n", " ")         // Newlines break tables
                    .replace("\r", "");         // Remove carriage returns
            };

            QString message = escapeMarkdownCell(QString::fromStdString(event->findByKey("message")).left(100));
            QString actor = escapeMarkdownCell(QString::fromStdString(event->findByKey("actor")));

            md += "| " + timestamp + " | " + level + " | " + message + " | " + actor + " |\n";
        }
    }

    return md;
}

QString ReportGenerator::GenerateJSON(const std::vector<const db::LogEvent*>& events, const ReportOptions& options)
{
    nlohmann::json j;
    j["title"] = options.title.toStdString();
    j["generated"] = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss").toStdString();
    j["eventCount"] = events.size();

    // Statistics
    auto stats = CalculateStatistics(events);
    j["statistics"] = {
        {"totalEvents", stats.totalEvents},
        {"criticalCount", stats.criticalCount},
        {"errorCount", stats.errorCount},
        {"warningCount", stats.warningCount},
        {"infoCount", stats.infoCount},
        {"timeSpanMs", stats.timeSpanMs}
    };

    // Events
    j["events"] = nlohmann::json::array();
    for (const auto* event : events)
    {
        j["events"].push_back({
            {"timestamp", event->findByKey("timestamp")},
            {"level", event->findByKey("level")},
            {"message", event->findByKey("message")},
            {"actor", event->findByKey("actor")}
        });
    }

    return QString::fromStdString(j.dump(2));
}

QString ReportGenerator::GeneratePlainText(const std::vector<const db::LogEvent*>& events, const ReportOptions& options)
{
    auto stats = CalculateStatistics(events);

    QString txt = options.title + "\n";
    txt += QString(options.title.length(), '=') + "\n";
    txt += "\nGenerated: " + QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss") + "\n";
    txt += "\n" + GenerateSummary(stats) + "\n";
    txt += "\nStatistics:\n";
    txt += "- Total Events: " + QString::number(stats.totalEvents) + "\n";
    txt += "- Critical: " + QString::number(stats.criticalCount) + "\n";
    txt += "- Errors: " + QString::number(stats.errorCount) + "\n";
    txt += "- Warnings: " + QString::number(stats.warningCount) + "\n";
    txt += "- Info: " + QString::number(stats.infoCount) + "\n";

    return txt;
}

ReportGenerator::ReportStatistics ReportGenerator::CalculateStatistics(const std::vector<const db::LogEvent*>& events)
{
    ReportStatistics stats{};
    stats.totalEvents = static_cast<int>(events.size());

    std::set<QString> uniqueActorSet;  // O(log n) insertion instead of O(n) search

    for (const auto* event : events)
    {
        QString level = QString::fromStdString(event->findByKey("level")).toUpper();
        stats.levelDistribution[level]++;

        if (level == "CRITICAL") stats.criticalCount++;
        else if (level == "ERROR") stats.errorCount++;
        else if (level == "WARNING") stats.warningCount++;
        else if (level == "INFO") stats.infoCount++;
        else if (level == "DEBUG") stats.debugCount++;

        QString actor = QString::fromStdString(event->findByKey("actor"));
        if (!actor.isEmpty()) {
            uniqueActorSet.insert(actor);  // O(log n), not O(n)
        }
    }

    // Convert set to vector (now properly deduplicated)
    stats.uniqueActors = std::vector<QString>(uniqueActorSet.begin(), uniqueActorSet.end());

    // Calculate actual time span from timestamps
    stats.timeSpanMs = 0;
    if (events.size() > 1) {
        QDateTime firstTime, lastTime;

        for (const auto* event : events) {
            QString tsStr = QString::fromStdString(event->findByKey("timestamp"));
            if (tsStr.isEmpty()) continue;

            QDateTime dt = QDateTime::fromString(tsStr, Qt::ISODate);
            if (!dt.isValid()) {
                dt = QDateTime::fromString(tsStr, "yyyy-MM-dd hh:mm:ss");
            }

            if (dt.isValid()) {
                if (!firstTime.isValid()) firstTime = dt;
                lastTime = dt;
            }
        }

        if (firstTime.isValid() && lastTime.isValid()) {
            stats.timeSpanMs = firstTime.msecsTo(lastTime);
        }
    }

    return stats;
}

QString ReportGenerator::GenerateSummary(const ReportStatistics& stats)
{
    return QString("Total events analyzed: %1\n"
                  "Critical: %2 | Errors: %3 | Warnings: %4 | Info: %5\n"
                  "Time span: %6 minutes\n"
                  "Unique actors: %7")
        .arg(stats.totalEvents)
        .arg(stats.criticalCount)
        .arg(stats.errorCount)
        .arg(stats.warningCount)
        .arg(stats.infoCount)
        .arg(stats.timeSpanMs / 60000)
        .arg(stats.uniqueActors.size());
}

QString ReportGenerator::GenerateStatisticsTable(const ReportStatistics& stats)
{
    QString table = "| Metric | Value |\n|--------|-------|\n";
    table += "| Total Events | " + QString::number(stats.totalEvents) + " |\n";
    table += "| Critical | " + QString::number(stats.criticalCount) + " |\n";
    table += "| Errors | " + QString::number(stats.errorCount) + " |\n";
    table += "| Warnings | " + QString::number(stats.warningCount) + " |\n";
    table += "| Info | " + QString::number(stats.infoCount) + " |\n";
    table += "| Time Span | " + QString::number(stats.timeSpanMs / 60000) + " minutes |\n";
    table += "| Unique Actors | " + QString::number(stats.uniqueActors.size()) + " |\n";
    return table;
}

QString ReportGenerator::GenerateTimeline(const std::vector<const db::LogEvent*>& events)
{
    // Placeholder for timeline generation
    return "Timeline data would be generated here";
}

QString ReportGenerator::GenerateTrendAnalysis(const std::vector<const db::LogEvent*>&)
{
    // Placeholder for trend analysis
    return "Trend analysis would be generated here";
}

} // namespace ui::qt::utils
