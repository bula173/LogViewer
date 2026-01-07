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
/**
 * @test Test merge with equal timestamps uses event ID as tiebreaker
 * 
 * When multiple events have the same timestamp, they should be ordered
 * by event ID to ensure deterministic ordering. After merge, IDs are
 * reassigned sequentially (0-5) and original IDs are stored.
 */
TEST(EventsContainerTest, MergeEventsEqualTimestampsWithIdTiebreaker)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Both containers have events with the same timestamp
    const std::string sameTime = "2025-01-01T10:00:00";
    
    // Container1: events with IDs 1, 3, 5
    container1.AddEvent({1, {{"timestamp", sameTime}, {"message", "Event1_ID1"}}});
    container1.AddEvent({3, {{"timestamp", sameTime}, {"message", "Event3_ID3"}}});
    container1.AddEvent({5, {{"timestamp", sameTime}, {"message", "Event5_ID5"}}});
    
    // Container2: events with IDs 2, 4, 6
    container2.AddEvent({2, {{"timestamp", sameTime}, {"message", "Event2_ID2"}}});
    container2.AddEvent({4, {{"timestamp", sameTime}, {"message", "Event4_ID4"}}});
    container2.AddEvent({6, {{"timestamp", sameTime}, {"message", "Event6_ID6"}}});

    container1.MergeEvents(container2, "existing", "new", "timestamp");

    // After merge with ID tiebreaker, events are ordered by ID: 1, 2, 3, 4, 5, 6
    // Then IDs are reassigned sequentially: 0, 1, 2, 3, 4, 5
    ASSERT_EQ(container1.Size(), 6);
    
    // Verify sequential IDs after reassignment
    EXPECT_EQ(container1.GetEvent(0).getId(), 0);
    EXPECT_EQ(container1.GetEvent(1).getId(), 1);
    EXPECT_EQ(container1.GetEvent(2).getId(), 2);
    EXPECT_EQ(container1.GetEvent(3).getId(), 3);
    EXPECT_EQ(container1.GetEvent(4).getId(), 4);
    EXPECT_EQ(container1.GetEvent(5).getId(), 5);
    
    // Verify original IDs are stored
    EXPECT_EQ(container1.GetEvent(0).findByKey("original_id"), "1");
    EXPECT_EQ(container1.GetEvent(1).findByKey("original_id"), "2");
    EXPECT_EQ(container1.GetEvent(2).findByKey("original_id"), "3");
    EXPECT_EQ(container1.GetEvent(3).findByKey("original_id"), "4");
    EXPECT_EQ(container1.GetEvent(4).findByKey("original_id"), "5");
    EXPECT_EQ(container1.GetEvent(5).findByKey("original_id"), "6");
}

/**
 * @test Test merge stores original_id for all events
 * 
 * Verifies that when merging containers, ALL events have their original ID
 * stored in the original_id field before IDs are reassigned sequentially.
 */
TEST(EventsContainerTest, MergeEventsStoresOriginalId)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Container1 events (existing)
    container1.AddEvent({1, {{"timestamp", "2025-01-01T10:00:00"}, {"message", "Event1"}}});
    container1.AddEvent({2, {{"timestamp", "2025-01-01T10:02:00"}, {"message", "Event2"}}});

    // Container2 events (new) - with same IDs as in container1
    container2.AddEvent({1, {{"timestamp", "2025-01-01T10:01:00"}, {"message", "Event3"}}});
    container2.AddEvent({2, {{"timestamp", "2025-01-01T10:03:00"}, {"message", "Event4"}}});

    container1.MergeEvents(container2, "existing", "new", "timestamp");

    ASSERT_EQ(container1.Size(), 4);

    // After merge, all events should have original_id set (preserving pre-merge IDs)
    // and IDs should be reassigned sequentially 0,1,2,3
    auto& event0 = container1.GetEvent(0);
    EXPECT_EQ(event0.getId(), 0);  // New sequential ID
    EXPECT_EQ(event0.GetSource(), "existing");
    EXPECT_EQ(event0.findByKey("original_id"), "1");  // Original ID from before merge
    
    auto& event1 = container1.GetEvent(1);
    EXPECT_EQ(event1.getId(), 1);  // New sequential ID
    EXPECT_EQ(event1.GetSource(), "new");
    EXPECT_EQ(event1.findByKey("original_id"), "1");  // Original ID from before merge
    
    auto& event2 = container1.GetEvent(2);
    EXPECT_EQ(event2.getId(), 2);  // New sequential ID
    EXPECT_EQ(event2.GetSource(), "existing");
    EXPECT_EQ(event2.findByKey("original_id"), "2");  // Original ID from before merge
    
    auto& event3 = container1.GetEvent(3);
    EXPECT_EQ(event3.getId(), 3);  // New sequential ID
    EXPECT_EQ(event3.GetSource(), "new");
    EXPECT_EQ(event3.findByKey("original_id"), "2");  // Original ID from before merge
}

/**
 * @test Test merge with mixed timestamp order and ID tiebreaker
 * 
 * Complex scenario: events with different timestamps, and when timestamps
 * match, the ID tiebreaker determines order. After merge, IDs are reassigned
 * sequentially and original IDs are stored.
 */
TEST(EventsContainerTest, MergeEventsMixedTimestampsAndIds)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    // Container1: times 10:00 (ID=1,3), 10:02 (ID=5)
    container1.AddEvent({1, {{"timestamp", "2025-01-01T10:00:00"}, {"message", "E1"}}});
    container1.AddEvent({3, {{"timestamp", "2025-01-01T10:00:00"}, {"message", "E3"}}});
    container1.AddEvent({5, {{"timestamp", "2025-01-01T10:02:00"}, {"message", "E5"}}});

    // Container2: times 10:00 (ID=2), 10:01 (ID=4)
    container2.AddEvent({2, {{"timestamp", "2025-01-01T10:00:00"}, {"message", "E2"}}});
    container2.AddEvent({4, {{"timestamp", "2025-01-01T10:01:00"}, {"message", "E4"}}});

    container1.MergeEvents(container2, "existing", "new", "timestamp");

    ASSERT_EQ(container1.Size(), 5);

    // Expected order after merge by timestamp+ID: 
    // 10:00: IDs 1,2,3 (ordered by ID)
    // 10:01: ID 4
    // 10:02: ID 5
    // Then reassigned to sequential IDs 0-4 with original_id stored
    EXPECT_EQ(container1.GetEvent(0).getId(), 0);
    EXPECT_EQ(container1.GetEvent(0).findByKey("original_id"), "1");
    EXPECT_EQ(container1.GetEvent(0).findByKey("timestamp"), "2025-01-01T10:00:00");

    EXPECT_EQ(container1.GetEvent(1).getId(), 1);
    EXPECT_EQ(container1.GetEvent(1).findByKey("original_id"), "2");
    EXPECT_EQ(container1.GetEvent(1).findByKey("timestamp"), "2025-01-01T10:00:00");

    EXPECT_EQ(container1.GetEvent(2).getId(), 2);
    EXPECT_EQ(container1.GetEvent(2).findByKey("original_id"), "3");
    EXPECT_EQ(container1.GetEvent(2).findByKey("timestamp"), "2025-01-01T10:00:00");

    EXPECT_EQ(container1.GetEvent(3).getId(), 3);
    EXPECT_EQ(container1.GetEvent(3).findByKey("original_id"), "4");
    EXPECT_EQ(container1.GetEvent(3).findByKey("timestamp"), "2025-01-01T10:01:00");

    EXPECT_EQ(container1.GetEvent(4).getId(), 4);
    EXPECT_EQ(container1.GetEvent(4).findByKey("original_id"), "5");
    EXPECT_EQ(container1.GetEvent(4).findByKey("timestamp"), "2025-01-01T10:02:00");
}

/**
 * @test Test ID tiebreaker with many events at same timestamp
 * 
 * Stress test: multiple events from both containers all have the same timestamp.
 * They should be ordered entirely by event ID, then reassigned to sequential IDs
 * with original IDs stored.
 */
TEST(EventsContainerTest, MergeEventsManyEventsEqualTimestamps)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    const std::string sameTime = "2025-01-01T10:00:00";

    // Container1: add events with IDs 2, 4, 6, 8 at same timestamp
    container1.AddEvent({2, {{"timestamp", sameTime}, {"message", "Event_ID2"}}});
    container1.AddEvent({4, {{"timestamp", sameTime}, {"message", "Event_ID4"}}});
    container1.AddEvent({6, {{"timestamp", sameTime}, {"message", "Event_ID6"}}});
    container1.AddEvent({8, {{"timestamp", sameTime}, {"message", "Event_ID8"}}});

    // Container2: add events with IDs 1, 3, 5, 7, 9 at same timestamp
    container2.AddEvent({1, {{"timestamp", sameTime}, {"message", "Event_ID1"}}});
    container2.AddEvent({3, {{"timestamp", sameTime}, {"message", "Event_ID3"}}});
    container2.AddEvent({5, {{"timestamp", sameTime}, {"message", "Event_ID5"}}});
    container2.AddEvent({7, {{"timestamp", sameTime}, {"message", "Event_ID7"}}});
    container2.AddEvent({9, {{"timestamp", sameTime}, {"message", "Event_ID9"}}});

    container1.MergeEvents(container2, "src1", "src2", "timestamp");

    ASSERT_EQ(container1.Size(), 9);

    // All events should be ordered by ID and then reassigned to sequential IDs 0-8
    // with original IDs stored (1, 2, 3, 4, 5, 6, 7, 8, 9)
    for (int i = 0; i < 9; ++i)
    {
        EXPECT_EQ(container1.GetEvent(i).getId(), i) 
            << "Event at index " << i << " should have sequential ID " << i;
        EXPECT_EQ(container1.GetEvent(i).findByKey("original_id"), std::to_string(i + 1))
            << "Event at index " << i << " should have original_id " << (i + 1);
        EXPECT_EQ(container1.GetEvent(i).findByKey("timestamp"), sameTime)
            << "All events should have same timestamp";
    }
}

/**
 * @test Test ID tiebreaker with alternating timestamps
 * 
 * Events alternate between two timestamps. Within each timestamp group,
 * they should be ordered by ID. After merge, IDs are reassigned sequentially
 * with original IDs stored.
 */
TEST(EventsContainerTest, MergeEventsAlternatingTimestampsWithIdOrder)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    const std::string time1 = "2025-01-01T10:00:00";
    const std::string time2 = "2025-01-01T10:01:00";

    // Container1: IDs 1,2 at time1; IDs 5,6 at time2
    container1.AddEvent({1, {{"timestamp", time1}, {"message", "E1"}}});
    container1.AddEvent({2, {{"timestamp", time1}, {"message", "E2"}}});
    container1.AddEvent({5, {{"timestamp", time2}, {"message", "E5"}}});
    container1.AddEvent({6, {{"timestamp", time2}, {"message", "E6"}}});

    // Container2: IDs 3,4 at time1; IDs 7,8 at time2
    container2.AddEvent({3, {{"timestamp", time1}, {"message", "E3"}}});
    container2.AddEvent({4, {{"timestamp", time1}, {"message", "E4"}}});
    container2.AddEvent({7, {{"timestamp", time2}, {"message", "E7"}}});
    container2.AddEvent({8, {{"timestamp", time2}, {"message", "E8"}}});

    container1.MergeEvents(container2, "src1", "src2", "timestamp");

    ASSERT_EQ(container1.Size(), 8);

    // Expected order after merge with ID tiebreaker: 
    // time1 with original IDs 1,2,3,4; then time2 with original IDs 5,6,7,8
    // Reassigned to sequential IDs 0-7
    EXPECT_EQ(container1.GetEvent(0).getId(), 0);
    EXPECT_EQ(container1.GetEvent(0).findByKey("original_id"), "1");
    EXPECT_EQ(container1.GetEvent(0).findByKey("timestamp"), time1);

    EXPECT_EQ(container1.GetEvent(1).getId(), 1);
    EXPECT_EQ(container1.GetEvent(1).findByKey("original_id"), "2");
    EXPECT_EQ(container1.GetEvent(1).findByKey("timestamp"), time1);

    EXPECT_EQ(container1.GetEvent(2).getId(), 2);
    EXPECT_EQ(container1.GetEvent(2).findByKey("original_id"), "3");
    EXPECT_EQ(container1.GetEvent(2).findByKey("timestamp"), time1);

    EXPECT_EQ(container1.GetEvent(3).getId(), 3);
    EXPECT_EQ(container1.GetEvent(3).findByKey("original_id"), "4");
    EXPECT_EQ(container1.GetEvent(3).findByKey("timestamp"), time1);

    EXPECT_EQ(container1.GetEvent(4).getId(), 4);
    EXPECT_EQ(container1.GetEvent(4).findByKey("original_id"), "5");
    EXPECT_EQ(container1.GetEvent(4).findByKey("timestamp"), time2);

    EXPECT_EQ(container1.GetEvent(5).getId(), 5);
    EXPECT_EQ(container1.GetEvent(5).findByKey("original_id"), "6");
    EXPECT_EQ(container1.GetEvent(5).findByKey("timestamp"), time2);

    EXPECT_EQ(container1.GetEvent(6).getId(), 6);
    EXPECT_EQ(container1.GetEvent(6).findByKey("original_id"), "7");
    EXPECT_EQ(container1.GetEvent(6).findByKey("timestamp"), time2);

    EXPECT_EQ(container1.GetEvent(7).getId(), 7);
    EXPECT_EQ(container1.GetEvent(7).findByKey("original_id"), "8");
    EXPECT_EQ(container1.GetEvent(7).findByKey("timestamp"), time2);
}

/**
 * @test Test ID tiebreaker preserves events from both containers correctly
 * 
 * When events have the same timestamp, source information should still be preserved
 * and original_id should be set correctly for all events. IDs are reassigned sequentially.
 */
TEST(EventsContainerTest, MergeEventsSameTimestampSourcePreservation)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    const std::string sameTime = "2025-01-01T10:00:00";

    // Container1: even IDs
    container1.AddEvent({2, {{"timestamp", sameTime}, {"message", "From_Src1_ID2"}}});
    container1.AddEvent({4, {{"timestamp", sameTime}, {"message", "From_Src1_ID4"}}});

    // Container2: odd IDs
    container2.AddEvent({1, {{"timestamp", sameTime}, {"message", "From_Src2_ID1"}}});
    container2.AddEvent({3, {{"timestamp", sameTime}, {"message", "From_Src2_ID3"}}});

    container1.MergeEvents(container2, "source1", "source2", "timestamp");

    ASSERT_EQ(container1.Size(), 4);

    // Events are merged by ID and reassigned sequential IDs 0-3
    EXPECT_EQ(container1.GetEvent(0).getId(), 0);
    EXPECT_EQ(container1.GetEvent(1).getId(), 1);
    EXPECT_EQ(container1.GetEvent(2).getId(), 2);
    EXPECT_EQ(container1.GetEvent(3).getId(), 3);

    // Verify sources are correct (based on original order by ID: 1, 2, 3, 4)
    EXPECT_EQ(container1.GetEvent(0).GetSource(), "source2");  // Original ID 1 from container2
    EXPECT_EQ(container1.GetEvent(1).GetSource(), "source1");  // Original ID 2 from container1
    EXPECT_EQ(container1.GetEvent(2).GetSource(), "source2");  // Original ID 3 from container2
    EXPECT_EQ(container1.GetEvent(3).GetSource(), "source1");  // Original ID 4 from container1

    // Verify original_id is set for all events
    EXPECT_EQ(container1.GetEvent(0).findByKey("original_id"), "1");  // From container2
    EXPECT_EQ(container1.GetEvent(1).findByKey("original_id"), "2");  // From container1
    EXPECT_EQ(container1.GetEvent(2).findByKey("original_id"), "3");  // From container2
    EXPECT_EQ(container1.GetEvent(3).findByKey("original_id"), "4");  // From container1
}

/**
 * @test Test ID tiebreaker with decreasing IDs at same timestamp
 * 
 * Events added in reverse ID order. Verifies that merge properly uses the ID
 * tiebreaker when all events have the same timestamp. IDs are reassigned sequentially
 * with original IDs stored.
 */
TEST(EventsContainerTest, MergeEventsSameTimestampReverseIds)
{
    db::EventsContainer container1;
    db::EventsContainer container2;

    const std::string sameTime = "2025-01-01T10:00:00";

    // Container1: add in reverse order (high to low IDs)
    container1.AddEvent({9, {{"timestamp", sameTime}, {"message", "ID9"}}});
    container1.AddEvent({7, {{"timestamp", sameTime}, {"message", "ID7"}}});
    container1.AddEvent({5, {{"timestamp", sameTime}, {"message", "ID5"}}});

    // Container2: also add in reverse order
    container2.AddEvent({8, {{"timestamp", sameTime}, {"message", "ID8"}}});
    container2.AddEvent({6, {{"timestamp", sameTime}, {"message", "ID6"}}});
    container2.AddEvent({4, {{"timestamp", sameTime}, {"message", "ID4"}}});

    container1.MergeEvents(container2, "src1", "src2", "timestamp");

    ASSERT_EQ(container1.Size(), 6);

    // Verify all have same timestamp
    for (int i = 0; i < 6; ++i)
    {
        EXPECT_EQ(container1.GetEvent(i).findByKey("timestamp"), sameTime);
    }
    
    // Collect all original IDs to verify they're preserved correctly
    std::set<int> foundOriginalIds;
    std::set<int> foundCurrentIds;
    for (int i = 0; i < 6; ++i)
    {
        foundCurrentIds.insert(container1.GetEvent(i).getId());
        foundOriginalIds.insert(std::stoi(container1.GetEvent(i).findByKey("original_id")));
    }
    
    // Verify sequential IDs 0-5
    EXPECT_EQ(foundCurrentIds.size(), 6);
    for (int id = 0; id < 6; ++id)
    {
        EXPECT_TRUE(foundCurrentIds.count(id)) << "Sequential ID " << id << " should be present";
    }
    
    // Verify all original IDs 4-9 are preserved
    EXPECT_EQ(foundOriginalIds.size(), 6);
    for (int id = 4; id <= 9; ++id)
    {
        EXPECT_TRUE(foundOriginalIds.count(id)) << "Original ID " << id << " should be present";
    }
}
