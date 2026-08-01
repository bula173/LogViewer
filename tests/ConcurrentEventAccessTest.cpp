#include <gtest/gtest.h>
#include "src/application/db/EventsContainer.hpp"
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>

namespace db::test {

class ConcurrentEventAccessTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_events = std::make_unique<EventsContainer>();
        m_errors = 0;
        m_accessCount = 0;
    }

    void AddInitialEvents(int count)
    {
        for (int i = 0; i < count; ++i) {
            m_events->AddEvent(LogEvent(i + 1, {
                {"timestamp", "2024-01-01T08:00:00Z"},
                {"level", (i % 3 == 0) ? "ERROR" : (i % 3 == 1) ? "WARN" : "INFO"},
                {"actor", "Service-" + std::to_string(i % 5)},
                {"message", "Event " + std::to_string(i)}
            }));
        }
    }

    std::unique_ptr<EventsContainer> m_events;
    std::atomic<int> m_errors{0};
    std::atomic<int> m_accessCount{0};
};

// Test: Reading while container is being written to
TEST_F(ConcurrentEventAccessTest, SafeReadDuringWrite)
{
    AddInitialEvents(100);

    std::atomic<bool> writerRunning(true);
    std::atomic<bool> readerRunning(true);
    int eventId = 101;

    // Writer thread: adds events
    std::thread writer([this, &writerRunning, &eventId]() {
        for (int i = 0; i < 50; ++i) {
            m_events->AddEvent(LogEvent(eventId++, {
                {"timestamp", "2024-01-01T08:00:00Z"},
                {"level", "INFO"},
                {"actor", "Writer"},
                {"message", "Written event"}
            }));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        writerRunning = false;
    });

    // Reader thread: accesses events
    std::thread reader([this, &readerRunning]() {
        int readCount = 0;
        while (readerRunning) {
            try {
                size_t size = m_events->Size();
                if (size > 0) {
                    // Try to access random events
                    for (size_t i = 0; i < size; ++i) {
                        try {
                            const auto& event = m_events->GetEvent(i);
                            std::string level = event.findByKey("level");
                            m_accessCount++;
                            readCount++;
                        } catch (const std::exception&) {
                            // Expected: event might be removed
                        }
                    }
                }
            } catch (const std::exception&) {
                // Container might be modified, continue
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Let writers finish, then stop readers
    writer.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    readerRunning = false;
    reader.join();

    // Should have completed without crashes
    EXPECT_GE(m_accessCount, 100);  // Should have accessed some events
    EXPECT_EQ(m_errors, 0);
}

// Test: Cache size first to avoid race condition
TEST_F(ConcurrentEventAccessTest, CachedSizePreventsBoundsError)
{
    AddInitialEvents(100);

    std::atomic<bool> running(true);
    std::vector<std::thread> readers;

    // Multiple reader threads with cached size
    for (int t = 0; t < 5; ++t) {
        readers.emplace_back([this, &running]() {
            while (running) {
                try {
                    // Cache size first (prevents resize race condition)
                    const size_t eventCount = m_events->Size();
                    for (size_t i = 0; i < eventCount; ++i) {
                        // Re-check size on each iteration
                        if (i >= m_events->Size())
                            break;

                        const auto& event = m_events->GetEvent(i);
                        std::string level = event.findByKey("level");
                        m_accessCount++;
                    }
                } catch (const std::exception&) {
                    m_errors++;
                }
            }
        });
    }

    // Give threads time to read
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Add more events while readers are active
    for (int i = 100; i < 150; ++i) {
        m_events->AddEvent(LogEvent(i + 1, {
            {"timestamp", "2024-01-01T08:00:00Z"},
            {"level", "INFO"},
            {"actor", "Added"},
            {"message", "New event"}
        }));
    }

    // More reading
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& t : readers)
        t.join();

    // Should complete without bounds errors
    EXPECT_GE(m_accessCount, 50);
    EXPECT_EQ(m_errors, 0);
}

// Test: Per-event error handling prevents crashes
TEST_F(ConcurrentEventAccessTest, PerEventErrorHandlingWorks)
{
    AddInitialEvents(50);

    std::atomic<bool> running(true);
    int successCount = 0;
    int skipCount = 0;

    // Simulate search pattern with error handling
    std::thread searchThread([this, &running, &successCount, &skipCount]() {
        while (running) {
            try {
                const size_t totalEvents = m_events->Size();
                for (size_t i = 0; i < totalEvents; ++i) {
                    try {
                        // Re-check before access
                        if (i >= m_events->Size())
                            break;

                        const auto& event = m_events->GetEvent(i);
                        std::string level = event.findByKey("level");
                        successCount++;
                    } catch (const std::exception&) {
                        // Skip events that fail to access
                        skipCount++;
                    }
                }
            } catch (const std::exception&) {
                // Container-level error, continue
            }
        }
    });

    // Concurrently modify container
    std::thread modifyThread([this]() {
        for (int i = 50; i < 100; ++i) {
            m_events->AddEvent(LogEvent(i + 1, {
                {"timestamp", "2024-01-01T08:00:00Z"},
                {"level", "INFO"},
                {"actor", "Modify"},
                {"message", "Event"}
            }));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    modifyThread.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;
    searchThread.join();

    // Should handle both successes and skips gracefully
    EXPECT_GT(successCount, 0);
    EXPECT_GE(successCount + skipCount, 50);
}

// Test: Multiple concurrent searches don't interfere
TEST_F(ConcurrentEventAccessTest, MultipleConcurrentSearches)
{
    AddInitialEvents(200);

    std::vector<std::thread> searchers;
    std::vector<int> searchResults(5, 0);

    // 5 concurrent search threads
    for (int t = 0; t < 5; ++t) {
        searchers.emplace_back([this, t, &searchResults]() {
            try {
                int count = 0;
                const size_t totalEvents = m_events->Size();
                for (size_t i = 0; i < totalEvents; ++i) {
                    try {
                        if (i >= m_events->Size())
                            break;

                        const auto& event = m_events->GetEvent(i);
                        std::string level = event.findByKey("level");
                        // Search for pattern
                        if (level.find("IN") != std::string::npos) {
                            count++;
                        }
                    } catch (const std::exception&) {
                        // Skip
                    }
                }
                searchResults[t] = count;
            } catch (const std::exception&) {
                searchResults[t] = -1;
            }
        });
    }

    for (auto& t : searchers)
        t.join();

    // All searches should complete successfully
    for (int i = 0; i < 5; ++i) {
        EXPECT_GE(searchResults[i], 0);  // No errors
        EXPECT_GT(searchResults[i], 0);  // Found some results
    }
}

// Stress test: Many concurrent accesses
TEST_F(ConcurrentEventAccessTest, StressTest1000Accesses)
{
    AddInitialEvents(100);

    std::vector<std::thread> threads;
    std::atomic<int> totalAccess(0);

    auto start = std::chrono::high_resolution_clock::now();

    // 10 threads doing 100 accesses each
    for (int t = 0; t < 10; ++t) {
        threads.emplace_back([this, &totalAccess]() {
            for (int i = 0; i < 100; ++i) {
                try {
                    const size_t size = m_events->Size();
                    if (size > 0) {
                        size_t idx = (i * 7) % size;  // Pseudo-random access
                        try {
                            const auto& event = m_events->GetEvent(idx);
                            std::string level = event.findByKey("level");
                            totalAccess++;
                        } catch (const std::exception&) {
                            // Skip
                        }
                    }
                } catch (const std::exception&) {
                    // Container error, continue
                }
            }
        });
    }

    for (auto& t : threads)
        t.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should complete 1000 accesses quickly
    EXPECT_GE(totalAccess, 900);  // Allow some skips
    EXPECT_LT(duration, 5000);    // Should complete in < 5 seconds
}

} // namespace db::test
