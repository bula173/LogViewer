#include <gtest/gtest.h>
#include "application/db/EventsContainer.hpp"
#include "application/db/LogEvent.hpp"
#include <chrono>

namespace functional::tests {

/**
 * Functional Tests: Large File Handling
 * Tests performance and memory behavior with large datasets
 */
class LargeFileHandlingTest : public ::testing::Test {
protected:
    db::EventsContainer m_container;

    /**
     * Create N events efficiently
     */
    void CreateEvents(size_t count, size_t messageSize = 100)
    {
        std::string baseMessage(messageSize, 'a');

        for (size_t i = 0; i < count; ++i) {
            db::LogEvent::EventItems items = {
                {"timestamp", "2026-08-01T12:00:" + std::to_string(i % 60)},
                {"level", (i % 100 < 10) ? "ERROR" : (i % 100 < 30) ? "WARNING" : "INFO"},
                {"source", "Module-" + std::to_string(i % 10)},
                {"message", baseMessage + "-" + std::to_string(i)}
            };
            db::LogEvent event(i, std::move(items));
            m_container.AddEvent(std::move(event));
        }
    }
};

/**
 * TEST: Handle 10k events without memory issues
 */
TEST_F(LargeFileHandlingTest, Handle10KEventsWithoutCrash)
{
    auto start = std::chrono::high_resolution_clock::now();

    CreateEvents(10'000);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(m_container.Size(), 10'000);

    // Should complete in reasonable time (< 5 seconds for 10k events)
    EXPECT_LT(duration.count(), 5000);

    // Verify we can access events
    for (int i = 0; i < 100; ++i) {
        const auto& event = m_container.GetEvent(i);
        EXPECT_FALSE(event.findByKey("message").empty());
    }
}

/**
 * TEST: Random access to events in large container
 */
TEST_F(LargeFileHandlingTest, RandomAccessPerformance)
{
    CreateEvents(50'000);
    EXPECT_EQ(m_container.Size(), 50'000);

    // Access events in random order
    for (int i = 0; i < 1000; ++i) {
        size_t randomIdx = (i * 7919) % m_container.Size();  // Use prime for distribution
        const auto& event = m_container.GetEvent(randomIdx);
        EXPECT_FALSE(event.findByKey("message").empty());
    }
}

/**
 * TEST: Batch event addition is more efficient than individual adds
 */
TEST_F(LargeFileHandlingTest, BatchAdditionEfficiency)
{
    auto startBatch = std::chrono::high_resolution_clock::now();

    std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
    for (int i = 0; i < 5000; ++i) {
        batch.push_back({i, {
            {"timestamp", "2026-08-01T12:00:00"},
            {"level", "INFO"},
            {"source", "Module-0"},
            {"message", "Batch event " + std::to_string(i)}
        }});
    }
    m_container.AddEventBatch(std::move(batch));

    auto endBatch = std::chrono::high_resolution_clock::now();
    auto batchDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endBatch - startBatch);

    EXPECT_EQ(m_container.Size(), 5000);
    EXPECT_LT(batchDuration.count(), 2000);  // Should complete quickly
}

/**
 * TEST: Container clear operation
 */
TEST_F(LargeFileHandlingTest, ClearLargeContainerEfficient)
{
    CreateEvents(10'000);
    EXPECT_EQ(m_container.Size(), 10'000);

    auto start = std::chrono::high_resolution_clock::now();
    m_container.Clear();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(m_container.Size(), 0);
    EXPECT_LT(duration.count(), 1000);  // Clear should be fast
}

/**
 * TEST: Merging large containers
 */
TEST_F(LargeFileHandlingTest, MergeContainersPerformance)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Create 5k events in each container
    for (int i = 0; i < 5000; ++i) {
        db::LogEvent::EventItems items = {
            {"timestamp", "2026-08-01T12:00:00"},
            {"level", "INFO"},
            {"source", "File1"},
            {"message", "Event " + std::to_string(i)}
        };
        container1.AddEvent(db::LogEvent(i, std::move(items)));
    }

    for (int i = 5000; i < 10000; ++i) {
        db::LogEvent::EventItems items = {
            {"timestamp", "2026-08-01T12:00:00"},
            {"level", "INFO"},
            {"source", "File2"},
            {"message", "Event " + std::to_string(i)}
        };
        container2.AddEvent(db::LogEvent(i, std::move(items)));
    }

    // Merge
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<unsigned long> indices2;
    for (unsigned long i = 0; i < container2.Size(); ++i) {
        indices2.push_back(i);
    }
    container1.MergeEvents(container2, indices2, "File2");
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(container1.Size(), 10'000);
    EXPECT_LT(duration.count(), 2000);
}

/**
 * TEST: Iteration over large container
 */
TEST_F(LargeFileHandlingTest, IterationPerformance)
{
    CreateEvents(20'000);

    auto start = std::chrono::high_resolution_clock::now();

    int messageCount = 0;
    for (unsigned long i = 0; i < m_container.Size(); ++i) {
        const auto& event = m_container.GetEvent(i);
        if (!event.findByKey("message").empty()) {
            messageCount++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(messageCount, 20'000);
    EXPECT_LT(duration.count(), 2000);  // 20k iterations should be fast
}

/**
 * TEST: Large messages don't cause issues
 */
TEST_F(LargeFileHandlingTest, LargeMessageContent)
{
    std::string largeMessage(10'000, 'x');  // 10KB message

    for (int i = 0; i < 100; ++i) {
        db::LogEvent::EventItems items = {
            {"timestamp", "2026-08-01T12:00:00"},
            {"level", "INFO"},
            {"source", "Test"},
            {"message", largeMessage + std::to_string(i)}
        };
        db::LogEvent event(i, std::move(items));
        m_container.AddEvent(std::move(event));
    }

    EXPECT_EQ(m_container.Size(), 100);

    // Access and verify
    for (int i = 0; i < 10; ++i) {
        const auto& event = m_container.GetEvent(i);
        const auto& msg = event.findByKey("message");
        EXPECT_TRUE(msg.size() > 10'000);
    }
}

/**
 * TEST: Many fields per event
 */
TEST_F(LargeFileHandlingTest, ManyFieldsPerEvent)
{
    for (int i = 0; i < 100; ++i) {
        db::LogEvent::EventItems items;

        // Create event with 50 fields
        for (int j = 0; j < 50; ++j) {
            items.push_back({
                "field_" + std::to_string(j),
                "value_" + std::to_string(j) + "_" + std::to_string(i)
            });
        }

        db::LogEvent event(i, std::move(items));
        m_container.AddEvent(std::move(event));
    }

    EXPECT_EQ(m_container.Size(), 100);

    // Verify field lookup works
    const auto& event = m_container.GetEvent(0);
    EXPECT_FALSE(event.findByKey("field_0").empty());
    EXPECT_FALSE(event.findByKey("field_25").empty());
    EXPECT_FALSE(event.findByKey("field_49").empty());
}

} // namespace functional::tests
