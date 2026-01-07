#include <gtest/gtest.h>
#include "EventsContainer.hpp"
#include "LogEvent.hpp"

TEST(EventsContainerTest, AddEvent)
{
    db::EventsContainer container;
    container.AddEvent(db::LogEvent(1, {{"key1", "value1"}, {"key2", "value2"}}));
    ASSERT_EQ(container.GetEvent(0), db::LogEvent(1, {{"key1", "value1"}, {"key2", "value2"}}));
}

TEST(EventsContainerTest, GetEvent)
{
    db::EventsContainer container;
    container.AddEvent({1, {{"key1", "value1"}, {"key2", "value2"}}});
    container.AddEvent({2, {{"key3", "value3"}, {"key4", "value4"}}});
    ASSERT_EQ(container.GetEvent(1), db::LogEvent(2, {{"key3", "value3"}, {"key4", "value4"}}));
}

// =============================================================================
// Tests for MergeEvents functionality
// =============================================================================

/**
 * @test Test basic merge of two containers with interleaved timestamps
 * 
 * This test verifies that logs from two containers are properly merged
 * in chronological order by timestamp.
 */
TEST(EventsContainerTest, MergeEventsBasic)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Container1: timestamps at 10:00 and 10:02
    container1.AddEvent({1, {{"timestamp", "2025-01-01T10:00:00"}, {"message", "Event1"}}});
    container1.AddEvent({2, {{"timestamp", "2025-01-01T10:02:00"}, {"message", "Event2"}}});

    // Container2: timestamps at 10:01 and 10:03
    container2.AddEvent({3, {{"timestamp", "2025-01-01T10:01:00"}, {"message", "Event3"}}});
    container2.AddEvent({4, {{"timestamp", "2025-01-01T10:03:00"}, {"message", "Event4"}}});

    // Merge container2 into container1
    container1.MergeEvents(container2, "source1", "source2", "timestamp");

    // After merge, container1 should have 4 events in timestamp order
    ASSERT_EQ(container1.Size(), 4);

    // Verify the order is correct: 10:00, 10:01, 10:02, 10:03
    auto& event0 = container1.GetEvent(0);
    auto& event1 = container1.GetEvent(1);
    auto& event2 = container1.GetEvent(2);
    auto& event3 = container1.GetEvent(3);

    EXPECT_EQ(event0.findByKey("timestamp"), "2025-01-01T10:00:00");
    EXPECT_EQ(event1.findByKey("timestamp"), "2025-01-01T10:01:00");
    EXPECT_EQ(event2.findByKey("timestamp"), "2025-01-01T10:02:00");
    EXPECT_EQ(event3.findByKey("timestamp"), "2025-01-01T10:03:00");

    // Verify source aliases are set correctly
    EXPECT_EQ(event0.GetSource(), "source1");
    EXPECT_EQ(event1.GetSource(), "source2");
    EXPECT_EQ(event2.GetSource(), "source1");
    EXPECT_EQ(event3.GetSource(), "source2");
}

/**
 * @test Test merge with different timestamp field names
 * 
 * Verifies that MergeEvents works correctly when timestamps are in
 * a custom field name (e.g., "time" instead of "timestamp").
 */
TEST(EventsContainerTest, MergeEventsCustomTimestampField)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Use custom field name "time" instead of "timestamp"
    container1.AddEvent({1, {{"time", "2025-01-01T10:00:00"}, {"message", "Event1"}}});
    container1.AddEvent({2, {{"time", "2025-01-01T10:02:00"}, {"message", "Event2"}}});

    container2.AddEvent({3, {{"time", "2025-01-01T10:01:00"}, {"message", "Event3"}}});
    container2.AddEvent({4, {{"time", "2025-01-01T10:03:00"}, {"message", "Event4"}}});

    // Merge with custom timestamp field
    container1.MergeEvents(container2, "src1", "src2", "time");

    // Verify the order is correct
    ASSERT_EQ(container1.Size(), 4);
    EXPECT_EQ(container1.GetEvent(0).findByKey("time"), "2025-01-01T10:00:00");
    EXPECT_EQ(container1.GetEvent(1).findByKey("time"), "2025-01-01T10:01:00");
    EXPECT_EQ(container1.GetEvent(2).findByKey("time"), "2025-01-01T10:02:00");
    EXPECT_EQ(container1.GetEvent(3).findByKey("time"), "2025-01-01T10:03:00");
}

/**
 * @test Test merge with missing timestamps
 * 
 * Verifies that events without timestamps are appended at the end
 * while maintaining their original order within their source.
 */
TEST(EventsContainerTest, MergeEventsMissingTimestamps)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Container1: one event with timestamp, one without
    container1.AddEvent({1, {{"timestamp", "2025-01-01T10:00:00"}, {"message", "Event1"}}});
    container1.AddEvent({2, {{"message", "Event2_NoTimestamp"}}});

    // Container2: one event with timestamp, one without
    container2.AddEvent({3, {{"timestamp", "2025-01-01T10:01:00"}, {"message", "Event3"}}});
    container2.AddEvent({4, {{"message", "Event4_NoTimestamp"}}});

    container1.MergeEvents(container2, "src1", "src2", "timestamp");

    // After merge, we should have: Event1, Event3, then events without timestamps
    // (Event2_NoTimestamp and Event4_NoTimestamp in original order within their sources)
    ASSERT_EQ(container1.Size(), 4);
    EXPECT_EQ(container1.GetEvent(0).findByKey("timestamp"), "2025-01-01T10:00:00");
    EXPECT_EQ(container1.GetEvent(0).findByKey("message"), "Event1");
    EXPECT_EQ(container1.GetEvent(1).findByKey("timestamp"), "2025-01-01T10:01:00");
    EXPECT_EQ(container1.GetEvent(1).findByKey("message"), "Event3");
    // Events without timestamps are appended at the end
    EXPECT_EQ(container1.GetEvent(2).findByKey("message"), "Event2_NoTimestamp");
    EXPECT_EQ(container1.GetEvent(3).findByKey("message"), "Event4_NoTimestamp");
}

/**
 * @test Test merge with equal timestamps preserves insertion order
 * 
 * When two events have the same timestamp, the one from the existing
 * container should come first (stable merge property).
 */
TEST(EventsContainerTest, MergeEventsEqualTimestamps)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Both events have the same timestamp
    const std::string sameTime = "2025-01-01T10:00:00";
    
    container1.AddEvent({1, {{"timestamp", sameTime}, {"message", "Event1_Existing"}}});
    container2.AddEvent({2, {{"timestamp", sameTime}, {"message", "Event2_New"}}});

    container1.MergeEvents(container2, "existing", "new", "timestamp");

    // The existing event should come before the new event (stable merge)
    ASSERT_EQ(container1.Size(), 2);
    EXPECT_EQ(container1.GetEvent(0).findByKey("message"), "Event1_Existing");
    EXPECT_EQ(container1.GetEvent(0).GetSource(), "existing");
    EXPECT_EQ(container1.GetEvent(1).findByKey("message"), "Event2_New");
    EXPECT_EQ(container1.GetEvent(1).GetSource(), "new");
}

/**
 * @test Test merge with empty container
 * 
 * Verifies that merging an empty container into another doesn't lose data.
 */
TEST(EventsContainerTest, MergeEventsEmptyContainer)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Only add events to container1
    container1.AddEvent({1, {{"timestamp", "2025-01-01T10:00:00"}, {"message", "Event1"}}});
    container1.AddEvent({2, {{"timestamp", "2025-01-01T10:01:00"}, {"message", "Event2"}}});

    // container2 is empty
    ASSERT_EQ(container2.Size(), 0);

    container1.MergeEvents(container2, "src1", "src2", "timestamp");

    // Container1 should still have 2 events in original order
    ASSERT_EQ(container1.Size(), 2);
    EXPECT_EQ(container1.GetEvent(0).findByKey("timestamp"), "2025-01-01T10:00:00");
    EXPECT_EQ(container1.GetEvent(1).findByKey("timestamp"), "2025-01-01T10:01:00");
}

/**
 * @test Test merge with many interleaved events
 * 
 * Stress test with many events from both containers that are heavily
 * interleaved by timestamp.
 */
TEST(EventsContainerTest, MergeEventsManyInterleaved)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Container1: even timestamps (10:00, 10:02, 10:04, 10:06, 10:08)
    for (int i = 0; i < 5; ++i)
    {
        std::string timestamp = "2025-01-01T10:" + std::string(2, '0' + (i*2)/10) + 
                               std::string(1, '0' + (i*2)%10) + ":00";
        container1.AddEvent({i*2 + 1, {{"timestamp", timestamp}, {"message", "Event_Src1_" + std::to_string(i)}}});
    }

    // Container2: odd timestamps (10:01, 10:03, 10:05, 10:07, 10:09)
    for (int i = 0; i < 5; ++i)
    {
        std::string timestamp = "2025-01-01T10:" + std::string(2, '0' + (i*2+1)/10) + 
                               std::string(1, '0' + (i*2+1)%10) + ":00";
        container2.AddEvent({i*2 + 2, {{"timestamp", timestamp}, {"message", "Event_Src2_" + std::to_string(i)}}});
    }

    container1.MergeEvents(container2, "src1", "src2", "timestamp");

    // Should have 10 events total
    ASSERT_EQ(container1.Size(), 10);

    // Verify all timestamps are in order
    std::string prevTimestamp = "";
    for (int i = 0; i < 10; ++i)
    {
        std::string currentTimestamp = container1.GetEvent(i).findByKey("timestamp");
        if (!prevTimestamp.empty())
        {
            EXPECT_LE(prevTimestamp, currentTimestamp) 
                << "Timestamps not in order at index " << i;
        }
        prevTimestamp = currentTimestamp;
    }
}

/**
 * @test Test merge preserves all event data
 * 
 * Verifies that merge doesn't lose any event data and correctly copies all fields.
 */
TEST(EventsContainerTest, MergeEventsPreservesAllData)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Complex event with many fields
    container1.AddEvent({1, {
        {"timestamp", "2025-01-01T10:00:00"},
        {"level", "INFO"},
        {"message", "First event"},
        {"component", "Module1"},
        {"details", "Some details"}
    }});

    container2.AddEvent({2, {
        {"timestamp", "2025-01-01T10:01:00"},
        {"level", "ERROR"},
        {"message", "Second event"},
        {"component", "Module2"},
        {"error_code", "ERR001"}
    }});

    container1.MergeEvents(container2, "local", "remote", "timestamp");

    ASSERT_EQ(container1.Size(), 2);

    // Verify first event preserved
    auto& event1 = container1.GetEvent(0);
    EXPECT_EQ(event1.findByKey("level"), "INFO");
    EXPECT_EQ(event1.findByKey("message"), "First event");
    EXPECT_EQ(event1.findByKey("component"), "Module1");
    EXPECT_EQ(event1.findByKey("details"), "Some details");

    // Verify second event preserved
    auto& event2 = container1.GetEvent(1);
    EXPECT_EQ(event2.findByKey("level"), "ERROR");
    EXPECT_EQ(event2.findByKey("message"), "Second event");
    EXPECT_EQ(event2.findByKey("component"), "Module2");
    EXPECT_EQ(event2.findByKey("error_code"), "ERR001");
}
