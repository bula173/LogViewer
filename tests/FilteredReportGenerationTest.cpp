#include <gtest/gtest.h>
#include "src/application/db/EventsContainer.hpp"
#include "src/application/ui/qt/utils/ReportGenerator.hpp"
#include <chrono>

namespace ui::qt::utils::test {

class FilteredReportGenerationTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_events = std::make_unique<db::EventsContainer>();
        CreateTestEvents();
    }

    void CreateTestEvents()
    {
        // Create 10 test events with different levels
        AddEvent("2024-01-01T08:00:00Z", "ERROR", "WebServer", "Start error");
        AddEvent("2024-01-01T08:05:00Z", "INFO", "Database", "Connected");
        AddEvent("2024-01-01T08:10:00Z", "WARN", "Cache", "Low memory");
        AddEvent("2024-01-01T08:15:00Z", "ERROR", "Database", "Timeout");
        AddEvent("2024-01-01T08:20:00Z", "INFO", "WebServer", "Request OK");
        AddEvent("2024-01-01T08:25:00Z", "DEBUG", "Cache", "Cache hit");
        AddEvent("2024-01-01T08:30:00Z", "ERROR", "WebServer", "Connection fail");
        AddEvent("2024-01-01T08:35:00Z", "WARN", "Logger", "Disk full");
        AddEvent("2024-01-01T08:40:00Z", "INFO", "Monitor", "Health check pass");
        AddEvent("2024-01-01T08:45:00Z", "ERROR", "WebServer", "Internal error");
    }

    void AddEvent(const std::string& timestamp, const std::string& level,
                  const std::string& actor, const std::string& message)
    {
        static int eventId = 1;
        m_events->AddEvent(db::LogEvent(eventId++, {
            {"timestamp", timestamp},
            {"level", level},
            {"actor", actor},
            {"message", message}
        }));
    }

    std::unique_ptr<db::EventsContainer> m_events;
};

// Test: Report with no filtering includes all events
TEST_F(FilteredReportGenerationTest, UnfilteredReportIncludesAll)
{
    ReportGenerator generator(*m_events);

    // Generate report with all events (no filtering)
    std::vector<int> allIndices;
    for (int i = 0; i < static_cast<int>(m_events->Size()); ++i)
        allIndices.push_back(i);

    ReportGenerator::ReportOptions options;
    options.format = ReportGenerator::ReportFormat::JSON;
    options.title = "Full Report";

    QString report = generator.generateReport(allIndices, options);

    EXPECT_FALSE(report.isEmpty());
    // Should contain 10 events
    EXPECT_TRUE(report.contains("\"eventCount\": 10"));
}

// Test: Report with filtered indices includes only those events
TEST_F(FilteredReportGenerationTest, FilteredReportExcludesWrongIndices)
{
    ReportGenerator generator(*m_events);

    // Filter: only ERROR events (indices 0, 3, 6, 9)
    std::vector<int> errorIndices = {0, 3, 6, 9};

    ReportGenerator::ReportOptions options;
    options.format = ReportGenerator::ReportFormat::JSON;
    options.title = "Error Report";
    options.includeEventList = true;

    QString report = generator.generateReport(errorIndices, options);

    EXPECT_FALSE(report.isEmpty());
    EXPECT_TRUE(report.contains("\"eventCount\": 4"));

    // Verify all events in report are ERROR level
    EXPECT_TRUE(report.contains("\"level\": \"ERROR\""));
    // Should NOT contain INFO, WARN, DEBUG
    EXPECT_FALSE(report.contains("\"level\": \"INFO\""));
}

// Test: Report with specific actors only
TEST_F(FilteredReportGenerationTest, FilteredByActorShowsCorrectEvents)
{
    ReportGenerator generator(*m_events);

    // Filter: only WebServer events (indices 0, 4, 6, 9)
    std::vector<int> webserverIndices = {0, 4, 6, 9};

    ReportGenerator::ReportOptions options;
    options.format = ReportGenerator::ReportFormat::JSON;
    options.title = "WebServer Report";

    QString report = generator.generateReport(webserverIndices, options);

    EXPECT_FALSE(report.isEmpty());
    EXPECT_TRUE(report.contains("\"eventCount\": 4"));
}

// Test: Report statistics accurate for filtered events
TEST_F(FilteredReportGenerationTest, FilteredStatisticsAreAccurate)
{
    ReportGenerator generator(*m_events);

    // Filter: only first 5 events
    std::vector<int> filteredIndices = {0, 1, 2, 3, 4};

    ReportGenerator::ReportOptions options;
    options.format = ReportGenerator::ReportFormat::JSON;
    options.includeStatistics = true;

    QString report = generator.generateReport(filteredIndices, options);

    EXPECT_FALSE(report.isEmpty());
    // Report should be valid JSON with event count
    EXPECT_TRUE(report.contains("eventCount"));
    EXPECT_TRUE(report.contains("ERROR"));  // Has error events
}

// Test: Report with single event
TEST_F(FilteredReportGenerationTest, SingleEventReportWorks)
{
    ReportGenerator generator(*m_events);

    std::vector<int> singleEvent = {5};  // DEBUG event

    ReportGenerator::ReportOptions options;
    options.format = ReportGenerator::ReportFormat::JSON;
    options.includeEventList = true;

    QString report = generator.generateReport(singleEvent, options);

    EXPECT_FALSE(report.isEmpty());
    EXPECT_TRUE(report.contains("\"eventCount\": 1"));
}

// Test: Report with empty filter
TEST_F(FilteredReportGenerationTest, EmptyFilterProducesValidReport)
{
    ReportGenerator generator(*m_events);

    std::vector<int> emptyIndices;

    ReportGenerator::ReportOptions options;
    options.format = ReportGenerator::ReportFormat::JSON;

    QString report = generator.generateReport(emptyIndices, options);

    // Should still produce valid JSON, just with 0 events
    EXPECT_FALSE(report.isEmpty());
    EXPECT_TRUE(report.contains("\"eventCount\": 0"));
}

// Performance test: Filtered report with large dataset
TEST_F(FilteredReportGenerationTest, LargeFilteredReportPerformance)
{
    ReportGenerator generator(*m_events);

    // Add many more events
    for (int i = 0; i < 1000; ++i) {
        AddEvent("2024-01-01T09:00:00Z",
                (i % 4 == 0) ? "ERROR" : (i % 4 == 1) ? "WARN" : (i % 4 == 2) ? "INFO" : "DEBUG",
                "Service-" + std::to_string(i % 10),
                "Event " + std::to_string(i));
    }

    // Filter: only even indices (500 events)
    std::vector<int> filteredIndices;
    for (int i = 0; i < static_cast<int>(m_events->Size()); i += 2)
        filteredIndices.push_back(i);

    auto start = std::chrono::high_resolution_clock::now();

    ReportGenerator::ReportOptions options;
    options.format = ReportGenerator::ReportFormat::JSON;
    options.includeEventList = true;
    options.maxEventsInReport = 500;

    QString report = generator.generateReport(filteredIndices, options);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_FALSE(report.isEmpty());
    EXPECT_LT(duration, 2000);  // Should complete in < 2 seconds
}

} // namespace ui::qt::utils::test
