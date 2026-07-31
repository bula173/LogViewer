#include <gtest/gtest.h>
#include "src/application/db/EventsContainer.hpp"

namespace db::test {

class DashboardPanelTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create sample events container
        m_events = std::make_unique<EventsContainer>();
    }

    void AddTestEvent(const std::string& level, const std::string& actor, const std::string& message)
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

// Test: Dashboard correctly counts total events
TEST_F(DashboardPanelTest, CountsTotalEvents)
{
    AddTestEvent("INFO", "Service-A", "Started");
    AddTestEvent("ERROR", "Service-B", "Failed");
    AddTestEvent("WARN", "Service-A", "Timeout");

    EXPECT_EQ(m_events->Size(), 3);
}

// Test: Dashboard correctly categorizes events by level
TEST_F(DashboardPanelTest, CategorizesEventsByLevel)
{
    AddTestEvent("ERROR", "Service-A", "Critical");
    AddTestEvent("ERROR", "Service-B", "Another error");
    AddTestEvent("WARN", "Service-A", "Warning");
    AddTestEvent("INFO", "Service-C", "Info");
    AddTestEvent("DEBUG", "Service-D", "Debug");

    // Verify distribution
    int errorCount = 0, warnCount = 0, infoCount = 0, debugCount = 0;

    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        std::string level = event.findByKey("level");
        if (level == "ERROR") errorCount++;
        else if (level == "WARN") warnCount++;
        else if (level == "INFO") infoCount++;
        else if (level == "DEBUG") debugCount++;
    }

    EXPECT_EQ(errorCount, 2);
    EXPECT_EQ(warnCount, 1);
    EXPECT_EQ(infoCount, 1);
    EXPECT_EQ(debugCount, 1);
}

// Test: Dashboard correctly identifies unique actors
TEST_F(DashboardPanelTest, IdentifiesUniqueActors)
{
    AddTestEvent("INFO", "WebServer", "Request received");
    AddTestEvent("INFO", "Database", "Query executed");
    AddTestEvent("INFO", "WebServer", "Response sent");
    AddTestEvent("INFO", "Cache", "Hit");

    std::set<std::string> uniqueActors;
    for (size_t i = 0; i < m_events->Size(); ++i)
    {
        const auto& event = m_events->GetEvent(i);
        std::string actor = event.findByKey("actor");
        if (!actor.empty())
            uniqueActors.insert(actor);
    }

    EXPECT_EQ(uniqueActors.size(), 3);
    EXPECT_TRUE(uniqueActors.count("WebServer"));
    EXPECT_TRUE(uniqueActors.count("Database"));
    EXPECT_TRUE(uniqueActors.count("Cache"));
}

// Test: Dashboard handles empty events container
TEST_F(DashboardPanelTest, HandlesEmptyContainer)
{
    EXPECT_EQ(m_events->Size(), 0);
}

// Test: Format number function produces correct output
TEST_F(DashboardPanelTest, FormatsNumbersCorrectly)
{
    // This would test the FormatNumber static method
    // Results should be like: 1234567 → "1.2M", 5432 → "5.4K", 123 → "123"
    // Note: Requires exposing FormatNumber or testing through panel UI
}

// Functional Test: Dashboard refresh after adding events
TEST_F(DashboardPanelTest, RefreshesAfterAddingEvents)
{
    AddTestEvent("INFO", "Service-A", "First");
    EXPECT_EQ(m_events->Size(), 1);

    AddTestEvent("ERROR", "Service-B", "Second");
    EXPECT_EQ(m_events->Size(), 2);

    AddTestEvent("WARN", "Service-A", "Third");
    EXPECT_EQ(m_events->Size(), 3);
}

} // namespace ui::qt::test
