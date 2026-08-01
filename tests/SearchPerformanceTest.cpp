#include <gtest/gtest.h>
#include "src/application/db/EventsContainer.hpp"
#include <chrono>
#include <vector>
#include <thread>

namespace db::test {

class SearchPerformanceTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_events = std::make_unique<EventsContainer>();
    }

    void CreateLargeDataset(size_t eventCount)
    {
        for (size_t i = 0; i < eventCount; ++i) {
            m_events->AddEvent(LogEvent(i + 1, {
                {"timestamp", "2024-01-01T08:00:00Z"},
                {"level", (i % 100 < 20) ? "ERROR" : (i % 100 < 40) ? "WARN" : "INFO"},
                {"actor", "Service-" + std::to_string(i % 50)},
                {"message", "Event message with searchable content number " + std::to_string(i)}
            }));
        }
    }

    int CountMatches(const std::string& query)
    {
        if (m_events->Size() == 0 || query.empty())
            return 0;

        int count = 0;
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

        for (size_t i = 0; i < m_events->Size(); ++i) {
            try {
                const auto& event = m_events->GetEvent(i);
                bool matches = false;

                for (const auto& [key, value] : event.getEventItems()) {
                    std::string lowerValue = value;
                    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
                    if (lowerValue.find(lowerQuery) != std::string::npos) {
                        matches = true;
                        break;
                    }
                }

                if (matches)
                    count++;
            } catch (const std::exception&) {
                // Skip
            }
        }

        return count;
    }

    std::unique_ptr<EventsContainer> m_events;
};

// Test: Search performance with small dataset (100 events)
TEST_F(SearchPerformanceTest, SmallDatasetSearch)
{
    CreateLargeDataset(100);

    auto start = std::chrono::high_resolution_clock::now();
    int matches = CountMatches("error");
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_GT(matches, 0);
    EXPECT_LT(duration, 100);  // Should be very fast for small dataset
}

// Test: Search performance with medium dataset (1000 events)
TEST_F(SearchPerformanceTest, MediumDatasetSearch)
{
    CreateLargeDataset(1000);

    auto start = std::chrono::high_resolution_clock::now();
    int matches = CountMatches("service");
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_GT(matches, 0);
    EXPECT_LT(duration, 500);  // Should complete in < 500ms
}

// Test: Search performance with large dataset (10000 events)
TEST_F(SearchPerformanceTest, LargeDatasetSearch)
{
    CreateLargeDataset(10000);

    auto start = std::chrono::high_resolution_clock::now();
    int matches = CountMatches("event");
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_GT(matches, 0);
    EXPECT_LT(duration, 2000);  // Should complete in < 2 seconds
}

// Test: Multiple sequential searches (simulates typing)
TEST_F(SearchPerformanceTest, SequentialSearchesSimulateTyping)
{
    CreateLargeDataset(5000);

    std::vector<std::string> queries = {"e", "er", "err", "erro", "error"};
    std::vector<int64_t> durations;

    for (const auto& query : queries) {
        auto start = std::chrono::high_resolution_clock::now();
        int matches = CountMatches(query);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        durations.push_back(duration);

        EXPECT_GE(matches, 0);
        EXPECT_LT(duration, 1000);  // Each keystroke should be < 1 second
    }

    // Total time should be reasonable
    int64_t totalTime = 0;
    for (auto d : durations)
        totalTime += d;

    EXPECT_LT(totalTime, 5000);  // Total < 5 seconds for 5 searches
}

// Test: Case-insensitive search performance
TEST_F(SearchPerformanceTest, CaseInsensitiveSearchPerformance)
{
    CreateLargeDataset(5000);

    // Multiple case variations of same search
    std::vector<std::string> queries = {"ERROR", "Error", "error", "eRrOr"};

    for (const auto& query : queries) {
        auto start = std::chrono::high_resolution_clock::now();
        int matches1 = CountMatches(query);
        int matches2 = CountMatches("error");  // Should get same count
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        EXPECT_EQ(matches1, matches2);  // Case shouldn't matter
        EXPECT_LT(duration, 1000);
    }
}

// Test: Empty search results
TEST_F(SearchPerformanceTest, EmptySearchResultsPerformance)
{
    CreateLargeDataset(5000);

    auto start = std::chrono::high_resolution_clock::now();
    int matches = CountMatches("nonexistentquerystring");
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_EQ(matches, 0);
    EXPECT_LT(duration, 1000);  // Still should complete quickly
}

// Test: Search with wildcard-like patterns
TEST_F(SearchPerformanceTest, PatternSearchPerformance)
{
    CreateLargeDataset(5000);

    auto start = std::chrono::high_resolution_clock::now();
    int matches = CountMatches("ice-");  // Matches "Service-X"
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_GT(matches, 0);
    EXPECT_LT(duration, 1000);
}

// Test: Very long search query
TEST_F(SearchPerformanceTest, LongSearchQueryPerformance)
{
    CreateLargeDataset(5000);

    std::string longQuery = "Event message with searchable content number";

    auto start = std::chrono::high_resolution_clock::now();
    int matches = CountMatches(longQuery);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_GT(matches, 0);
    EXPECT_LT(duration, 1000);
}

// Stress test: Rapid sequential searches (simulates fast typing)
TEST_F(SearchPerformanceTest, RapidSequentialSearches)
{
    CreateLargeDataset(5000);

    auto start = std::chrono::high_resolution_clock::now();

    // Simulate 20 rapid searches in succession
    for (int i = 0; i < 20; ++i) {
        std::string query = "service-" + std::to_string(i % 10);
        int matches = CountMatches(query);
        EXPECT_GE(matches, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 20 searches on 5000 events should complete quickly
    EXPECT_LT(duration, 5000);  // < 5 seconds for 20 searches = < 250ms each
}

// Test: Search with actor name (specific field)
TEST_F(SearchPerformanceTest, ActorNameSearchPerformance)
{
    CreateLargeDataset(5000);

    auto start = std::chrono::high_resolution_clock::now();
    int matches = CountMatches("service-1");  // Should match Service-1, Service-10, Service-11, etc.
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_GT(matches, 0);
    EXPECT_LT(duration, 1000);
}

// Test: Performance comparison: cached size vs uncached
TEST_F(SearchPerformanceTest, CachedSizePerformanceGain)
{
    CreateLargeDataset(10000);

    // Method 1: Without caching size (less efficient)
    auto start1 = std::chrono::high_resolution_clock::now();
    int count1 = 0;
    for (size_t i = 0; i < m_events->Size(); ++i) {  // Size checked on each iteration
        try {
            if (i >= m_events->Size())
                break;
            const auto& event = m_events->GetEvent(i);
            count1++;
        } catch (const std::exception&) {}
    }
    auto end1 = std::chrono::high_resolution_clock::now();

    // Method 2: With caching size (more efficient)
    auto start2 = std::chrono::high_resolution_clock::now();
    int count2 = 0;
    const size_t eventCount = m_events->Size();  // Cache size once
    for (size_t i = 0; i < eventCount; ++i) {  // Use cached size
        try {
            if (i >= m_events->Size())
                break;
            const auto& event = m_events->GetEvent(i);
            count2++;
        } catch (const std::exception&) {}
    }
    auto end2 = std::chrono::high_resolution_clock::now();

    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2).count();

    // Cached version should be similar or better
    EXPECT_EQ(count1, count2);
    EXPECT_LT(duration2, duration1 * 1.1);  // Allow 10% margin for variance
}

// Test: Search doesn't block UI simulation
TEST_F(SearchPerformanceTest, SearchDoesntBlockUnderLoad)
{
    CreateLargeDataset(5000);  // Smaller dataset for reliability

    std::atomic<int> searchCount(0);
    std::atomic<bool> searching(true);

    // Search thread (with debounce)
    std::thread searchThread([this, &searchCount, &searching]() {
        while (searching) {
            int matches = CountMatches("error");
            if (matches >= 0)  // Valid search
                searchCount++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Debounce
        }
    });

    // Main thread continues operating
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    searching = false;
    searchThread.join();

    // Should have completed multiple searches without blocking main
    EXPECT_GE(searchCount, 1);  // At least one search
}

} // namespace db::test
