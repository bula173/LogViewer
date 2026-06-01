#include <gtest/gtest.h>

#include "ui/qt/panels/SideBySidePanel.hpp"
#include "db/EventsContainer.hpp"
#include "db/LogEvent.hpp"

namespace ui::qt::test
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void FillContainer(db::EventsContainer& c,
                           const std::vector<std::string>& timestamps)
{
    for (int i = 0; i < static_cast<int>(timestamps.size()); ++i)
    {
        db::LogEvent::EventItems items;
        if (!timestamps[static_cast<size_t>(i)].empty())
            items.emplace_back("timestamp", timestamps[static_cast<size_t>(i)]);
        items.emplace_back("id", std::to_string(i));
        c.AddEvent(db::LogEvent(i, std::move(items)));
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(SideBySideSyncTest, EmptyContainerReturnsMinusOne)
{
    db::EventsContainer c;
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 1.0, 0), -1);
}

TEST(SideBySideSyncTest, SingleEventIsAlwaysNearest)
{
    db::EventsContainer c;
    FillContainer(c, {"5.0"});
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 0.0,   0), 0);
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 5.0,   0), 0);
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 100.0, 0), 0);
}

TEST(SideBySideSyncTest, ExactMatch)
{
    db::EventsContainer c;
    FillContainer(c, {"1.0", "2.0", "3.0", "4.0", "5.0"});
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 3.0, 0), 2);
}

TEST(SideBySideSyncTest, NearestBelow)
{
    // 2.4 is closer to 2.0 (row 1) than to 3.0 (row 2)
    db::EventsContainer c;
    FillContainer(c, {"1.0", "2.0", "3.0", "4.0", "5.0"});
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 2.4, 0), 1);
}

TEST(SideBySideSyncTest, NearestAbove)
{
    // 2.6 is closer to 3.0 (row 2)
    db::EventsContainer c;
    FillContainer(c, {"1.0", "2.0", "3.0", "4.0", "5.0"});
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 2.6, 0), 2);
}

TEST(SideBySideSyncTest, BelowFirstClampedToZero)
{
    db::EventsContainer c;
    FillContainer(c, {"1.0", "2.0", "3.0"});
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 0.0, 0), 0);
}

TEST(SideBySideSyncTest, AboveLastClampedToEnd)
{
    db::EventsContainer c;
    FillContainer(c, {"1.0", "2.0", "3.0"});
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 99.0, 0), 2);
}

TEST(SideBySideSyncTest, NoTimestampsFallsBackToFallbackRow)
{
    db::EventsContainer c;
    FillContainer(c, {"", "", "", ""});
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 5.0, 2), 2);
}

TEST(SideBySideSyncTest, FallbackRowClampedToRange)
{
    db::EventsContainer c;
    FillContainer(c, {"", ""});
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 5.0,  99), 1);
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 5.0,  -3), 0);
}

TEST(SideBySideSyncTest, LargeContainerBinarySearchPerformance)
{
    // 100 000 sorted entries — verifies O(log n) path is fast enough.
    db::EventsContainer c;
    std::vector<std::string> tss;
    tss.reserve(100000);
    for (int i = 0; i < 100000; ++i)
        tss.push_back(std::to_string(static_cast<double>(i) * 0.01));
    FillContainer(c, tss);

    // target ≈ 500.00 s → should land around row 50000
    const int row = SideBySidePanel::FindNearestByTimestamp(c, 500.0, 0);
    EXPECT_GE(row, 49990);
    EXPECT_LE(row, 50010);
}

TEST(SideBySideSyncTest, ManualOffsetApplied)
{
    // Right log timestamps are left + 10.0 — verify FindNearestByTimestamp
    // returns the correct row when an offset is pre-applied by the caller.
    db::EventsContainer c;
    FillContainer(c, {"10.0", "11.0", "12.0", "13.0", "14.0"});
    const double offset = 10.0;
    // Left row 2 has ts=2.0; with offset, target on right = 2.0 + 10.0 = 12.0 → row 2
    EXPECT_EQ(SideBySidePanel::FindNearestByTimestamp(c, 2.0 + offset, 0), 2);
}

} // namespace ui::qt::test
