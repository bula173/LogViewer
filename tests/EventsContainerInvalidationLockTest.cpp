// A background reader (the sequence diagram's actor discovery) holds
// LockAgainstInvalidation() while it scans: Clear() and MergeEvents() must
// wait for it, appends and reads must not.
#include <gtest/gtest.h>

#include "EventsContainer.hpp"

#include <atomic>
#include <chrono>
#include <thread>

namespace {

void Append(db::EventsContainer& events, int count)
{
    std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
    for (int i = 0; i < count; ++i)
        batch.push_back({i, {{"timestamp", "2026-01-01 00:00:0" + std::to_string(i % 10)}}});
    events.AddEventBatch(std::move(batch));
}

} // namespace

TEST(EventsContainerInvalidationLockTest, ClearWaitsForTheLock)
{
    db::EventsContainer events;
    Append(events, 10);

    std::atomic<bool> cleared {false};
    std::thread writer;
    {
        const auto keep = events.LockAgainstInvalidation();
        const db::LogEvent& first = events.GetEvent(0);
        writer = std::thread([&] { events.Clear(); cleared = true; });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_FALSE(cleared.load());
        EXPECT_EQ(first.getId(), 0); // still a live event
        EXPECT_EQ(events.Size(), 10u);
    }
    writer.join();
    EXPECT_TRUE(cleared.load());
    EXPECT_EQ(events.Size(), 0u);
}

TEST(EventsContainerInvalidationLockTest, MergeWaitsForTheLock)
{
    db::EventsContainer events;
    db::EventsContainer other;
    Append(events, 5);
    Append(other, 5);

    std::atomic<bool> merged {false};
    std::thread writer;
    {
        const auto keep = events.LockAgainstInvalidation();
        writer = std::thread([&] { events.MergeEvents(other, "a", "b"); merged = true; });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_FALSE(merged.load());
        EXPECT_EQ(events.Size(), 5u);
    }
    writer.join();
    EXPECT_TRUE(merged.load());
    EXPECT_EQ(events.Size(), 10u);
}

TEST(EventsContainerInvalidationLockTest, AppendsAndReadsAreNotBlocked)
{
    db::EventsContainer events;
    Append(events, 3);

    const auto keep = events.LockAgainstInvalidation();
    Append(events, 3); // same thread: must not deadlock
    EXPECT_EQ(events.Size(), 6u);
    EXPECT_NO_THROW((void)events.GetEvent(5));

    const auto second = events.LockAgainstInvalidation(); // readers share it
    EXPECT_TRUE(second.owns_lock());
}
