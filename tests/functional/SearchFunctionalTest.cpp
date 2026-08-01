#include <gtest/gtest.h>
#include "EventsContainer.hpp"
#include "LogEvent.hpp"
#include "MainController.hpp"

namespace functional::tests {

/**
 * Functional Tests: Search Workflow
 * Tests end-to-end search functionality with real data
 */
class SearchFunctionalTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create sample events
        for (int i = 0; i < 100; ++i) {
            db::LogEvent event;
            event.setId(i);
            event.addItem("timestamp", "2026-08-01 12:" + std::to_string(i % 60) + ":00");
            event.addItem("level", i % 10 == 0 ? "ERROR" : "INFO");
            event.addItem("source", i % 2 == 0 ? "Module-A" : "Module-B");
            event.addItem("message", "Test event " + std::to_string(i));

            if (i % 5 == 0)
                event.addItem("message", "CRITICAL error detected");

            m_events.AddEvent(std::move(event));
        }
    }

    db::EventsContainer m_events;
};

/**
 * TEST: Basic search returns correct count
 */
TEST_F(SearchFunctionalTest, SearchReturnsCorrectMatchCount)
{
    std::vector<mvc::SearchResultRow> results;
    auto append = [&results](const mvc::SearchResultRow& row) {
        results.push_back(row);
    };

    std::vector<std::string> columns = {"level", "source", "message"};

    // Search for "CRITICAL"
    mvc::MainController controller(m_events);
    controller.SearchEvents("CRITICAL", columns, append, nullptr);

    // Should find 20 events (100 events / 5 = 20 with CRITICAL)
    EXPECT_EQ(results.size(), 20);
}

/**
 * TEST: Empty search returns all events
 */
TEST_F(SearchFunctionalTest, EmptySearchReturnsAllEvents)
{
    std::vector<mvc::SearchResultRow> results;
    auto append = [&results](const mvc::SearchResultRow& row) {
        results.push_back(row);
    };

    std::vector<std::string> columns = {"level"};

    mvc::MainController controller(m_events);
    controller.SearchEvents("", columns, append, nullptr);

    EXPECT_EQ(results.size(), 100);
}

/**
 * TEST: Case-insensitive search works
 */
TEST_F(SearchFunctionalTest, CaseInsensitiveSearch)
{
    std::vector<mvc::SearchResultRow> results;
    auto append = [&results](const mvc::SearchResultRow& row) {
        results.push_back(row);
    };

    std::vector<std::string> columns = {"level"};

    mvc::MainController controller(m_events);
    // Search for lowercase "error", should find "ERROR"
    controller.SearchEvents("error", columns, append, nullptr);

    int errorCount = 0;
    for (const auto& result : results) {
        if (result.matchedText.find("ERROR") != std::string::npos)
            errorCount++;
    }

    EXPECT_GT(errorCount, 0);
}

/**
 * TEST: Progress callback is invoked during search
 */
TEST_F(SearchFunctionalTest, ProgressCallbackInvokedDuringSearch)
{
    int maxProgress = 0;
    auto progressCallback = [&maxProgress](std::size_t processed, std::size_t) {
        maxProgress = std::max(maxProgress, static_cast<int>(processed));
    };

    std::vector<std::string> columns = {"message"};

    mvc::MainController controller(m_events);
    controller.SearchEvents("test", columns, [](const mvc::SearchResultRow&) {}, progressCallback);

    // Progress should reach 100 (we have 100 events)
    EXPECT_EQ(maxProgress, 100);
}

/**
 * TEST: Search doesn't crash with special characters
 */
TEST_F(SearchFunctionalTest, SearchWithSpecialCharactersDoesNotCrash)
{
    std::vector<mvc::SearchResultRow> results;
    auto append = [&results](const mvc::SearchResultRow& row) {
        results.push_back(row);
    };

    std::vector<std::string> columns = {"message"};

    mvc::MainController controller(m_events);
    // These queries should not crash
    EXPECT_NO_THROW(controller.SearchEvents(".*", columns, append, nullptr));
    EXPECT_NO_THROW(controller.SearchEvents("[test]", columns, append, nullptr));
    EXPECT_NO_THROW(controller.SearchEvents("\\d+", columns, append, nullptr));
}

/**
 * TEST: Search handles null containers gracefully
 */
TEST_F(SearchFunctionalTest, SearchHandlesEmptyContainerGracefully)
{
    db::EventsContainer emptyEvents;

    std::vector<mvc::SearchResultRow> results;
    auto append = [&results](const mvc::SearchResultRow& row) {
        results.push_back(row);
    };

    std::vector<std::string> columns = {"message"};

    mvc::MainController controller(emptyEvents);
    EXPECT_NO_THROW(controller.SearchEvents("test", columns, append, nullptr));
    EXPECT_EQ(results.size(), 0);
}

} // namespace functional::tests
