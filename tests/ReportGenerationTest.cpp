#include <gtest/gtest.h>
#include "src/application/db/EventsContainer.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <chrono>

namespace db::test {

class ReportGenerationTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_events = std::make_unique<EventsContainer>();

        // Create diverse test events
        AddEvent("2024-01-01T08:00:00Z", "ERROR", "WebServer", "Service started with errors");
        AddEvent("2024-01-01T08:05:00Z", "INFO", "Database", "Connected successfully");
        AddEvent("2024-01-01T08:10:00Z", "WARN", "Cache", "Memory usage at 80%");
        AddEvent("2024-01-01T08:15:00Z", "ERROR", "Database", "Query timeout");
        AddEvent("2024-01-01T08:20:00Z", "INFO", "WebServer", "Request processed");
        AddEvent("2024-01-01T08:25:00Z", "DEBUG", "Cache", "Cache hit ratio: 95%");
        AddEvent("2024-01-01T08:30:00Z", "ERROR", "WebServer", "Connection refused");
        AddEvent("2024-01-01T08:35:00Z", "INFO", "Logger", "Log rotation completed");
    }

    void AddEvent(const std::string& timestamp, const std::string& level,
                  const std::string& actor, const std::string& message)
    {
        static int eventId = 1;
        m_events->AddEvent(LogEvent(eventId++, {
            {"timestamp", timestamp},
            {"level", level},
            {"actor", actor},
            {"message", message}
        }));
    }

    std::unique_ptr<EventsContainer> m_events;
};

// Test: Report data structure accuracy - verify event counts
TEST_F(ReportGenerationTest, AccurateEventCount)
{
    EXPECT_EQ(m_events->Size(), 8);
}

// Test: Report data structure - level distribution
TEST_F(ReportGenerationTest, LevelDistributionAccuracy)
{
    std::map<std::string, int> levelCounts;

    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        std::string level = event.findByKey("level");
        levelCounts[level]++;
    }

    // Expected: 3 ERROR, 3 INFO, 1 WARN, 1 DEBUG (see SetUp())
    EXPECT_EQ(levelCounts["ERROR"], 3);
    EXPECT_EQ(levelCounts["INFO"], 3);
    EXPECT_EQ(levelCounts["WARN"], 1);
    EXPECT_EQ(levelCounts["DEBUG"], 1);
}

// Test: Report data structure - unique actors
TEST_F(ReportGenerationTest, UniqueActorsIdentification)
{
    std::set<std::string> uniqueActors;

    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        std::string actor = event.findByKey("actor");
        if (!actor.empty())
            uniqueActors.insert(actor);
    }

    // Expected: WebServer, Database, Cache, Logger
    EXPECT_EQ(uniqueActors.size(), 4);
    EXPECT_TRUE(uniqueActors.count("WebServer"));
    EXPECT_TRUE(uniqueActors.count("Database"));
    EXPECT_TRUE(uniqueActors.count("Cache"));
    EXPECT_TRUE(uniqueActors.count("Logger"));
}

// Test: Report data structure - field extraction
TEST_F(ReportGenerationTest, FieldExtractionAccuracy)
{
    // Verify all required fields exist in events
    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        std::string timestamp = event.findByKey("timestamp");
        std::string level = event.findByKey("level");
        std::string actor = event.findByKey("actor");
        std::string message = event.findByKey("message");

        EXPECT_FALSE(timestamp.empty());
        EXPECT_FALSE(level.empty());
        EXPECT_FALSE(actor.empty());
        EXPECT_FALSE(message.empty());
    }
}

// Functional Test: Performance with large dataset
TEST_F(ReportGenerationTest, PerformanceWithManyEvents)
{
    // Add 500 more events
    for (int i = 0; i < 500; ++i)
    {
        AddEvent("2024-01-01T09:00:00Z", "INFO", "Service-" + std::to_string(i % 5),
                 "Event " + std::to_string(i));
    }

    EXPECT_EQ(m_events->Size(), 508);

    auto start = std::chrono::high_resolution_clock::now();

    // Iterate through all events
    int count = 0;
    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        std::string level = event.findByKey("level");
        if (!level.empty()) count++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_EQ(count, 508);
    EXPECT_LT(duration, 1000);  // Should complete in less than 1 second
}

} // namespace ui::qt::utils::test
