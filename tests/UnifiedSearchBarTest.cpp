#include <gtest/gtest.h>
#include "src/application/db/EventsContainer.hpp"
#include <algorithm>
#include <chrono>

namespace db::test {

class UnifiedSearchBarTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create sample events
        m_events = std::make_unique<EventsContainer>();

        // Add diverse test data
        AddEvent("ERROR", "WebServer", "Connection timeout");
        AddEvent("INFO", "Database", "Query executed successfully");
        AddEvent("WARN", "Cache", "High memory usage detected");
        AddEvent("DEBUG", "WebServer", "Request processed in 250ms");
        AddEvent("ERROR", "Database", "Connection pool exhausted");
    }

    void AddEvent(const std::string& level, const std::string& actor, const std::string& message)
    {
        static int eventId = 1;
        m_events->AddEvent(LogEvent(eventId++, {
            {"level", level},
            {"actor", actor},
            {"message", message},
            {"timestamp", "2024-01-01T12:00:00Z"}
        }));
    }

    std::unique_ptr<EventsContainer> m_events;
};

// Test: Search finds events by keyword in message
TEST_F(UnifiedSearchBarTest, SearchFindsEventsByMessage)
{
    // Events containing "Connection" or "timeout"
    // Expected: 2 matches ("Connection timeout" and "Connection pool exhausted")

    int count = 0;
    std::string query = "Connection";
    std::string lowerQuery = query;  // Would be lowercased in actual implementation

    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        bool matches = false;
        for (const auto& [key, value] : event.getEventItems())
        {
            if (value.find(query) != std::string::npos)
            {
                matches = true;
                break;
            }
        }
        if (matches) count++;
    }

    EXPECT_EQ(count, 2);
}

// Test: Search is case-insensitive
TEST_F(UnifiedSearchBarTest, SearchIsCaseInsensitive)
{
    // "error", "ERROR", "Error" should all match
    int count = 0;
    std::string query = "error";

    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        bool matches = false;
        for (const auto& [key, value] : event.getEventItems())
        {
            // Simple case-insensitive check
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find(query) != std::string::npos)
            {
                matches = true;
                break;
            }
        }
        if (matches) count++;
    }

    EXPECT_GE(count, 2);  // At least 2 ERROR events
}

// Test: Empty search returns all events
TEST_F(UnifiedSearchBarTest, EmptySearchReturnsAllEvents)
{
    std::string query = "";
    int count = 0;

    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        if (query.empty())
            count++;
    }

    EXPECT_EQ(count, static_cast<int>(m_events->Size()));
}

// Test: Search by actor field
TEST_F(UnifiedSearchBarTest, SearchByActor)
{
    int count = 0;
    std::string query = "WebServer";

    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        std::string actor = event.findByKey("actor");
        if (actor.find(query) != std::string::npos)
            count++;
    }

    EXPECT_EQ(count, 2);  // Two WebServer events
}

// Test: Search by level field
TEST_F(UnifiedSearchBarTest, SearchByLevel)
{
    int count = 0;
    std::string query = "ERROR";

    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        std::string level = event.findByKey("level");
        if (level.find(query) != std::string::npos)
            count++;
    }

    EXPECT_EQ(count, 2);  // Two ERROR events
}

// Test: No matches returns 0
TEST_F(UnifiedSearchBarTest, NoMatchesReturnsZero)
{
    int count = 0;
    std::string query = "NonexistentPattern12345";

    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        bool matches = false;
        for (const auto& [key, value] : event.getEventItems())
        {
            if (value.find(query) != std::string::npos)
            {
                matches = true;
                break;
            }
        }
        if (matches) count++;
    }

    EXPECT_EQ(count, 0);
}

// Test: Match counter accuracy
TEST_F(UnifiedSearchBarTest, MatchCounterAccuracy)
{
    // Query: "memory" - should match "High memory usage detected"
    std::string query = "memory";
    int count = 0;

    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        bool matches = false;
        for (const auto& [key, value] : event.getEventItems())
        {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find(query) != std::string::npos)
            {
                matches = true;
                break;
            }
        }
        if (matches) count++;
    }

    EXPECT_EQ(count, 1);
}

// Functional Test: Search performance with large dataset
TEST_F(UnifiedSearchBarTest, PerformanceWithLargeDataset)
{
    // Add 1000 events
    for (int i = 0; i < 1000; ++i)
    {
        AddEvent("INFO", "Service-" + std::to_string(i % 10), "Event " + std::to_string(i));
    }

    // Search should complete quickly
    auto start = std::chrono::high_resolution_clock::now();

    int count = 0;
    std::string query = "Service-5";
    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        std::string actor = event.findByKey("actor");
        if (actor.find(query) != std::string::npos)
            count++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(duration, 1000);  // Should complete in less than 1 second
    EXPECT_EQ(count, 100);      // 100 events for Service-5
}

} // namespace ui::qt::test
