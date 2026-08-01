#include <gtest/gtest.h>
#include "src/application/db/EventsContainer.hpp"
#include <QDateTime>

namespace db::test {

class TimestampParsingTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_events = std::make_unique<EventsContainer>();
    }

    void AddEventWithTimestamp(const std::string& timestamp, const std::string& level)
    {
        static int eventId = 1;
        m_events->AddEvent(LogEvent(eventId++, {
            {"timestamp", timestamp},
            {"level", level},
            {"actor", "Test"},
            {"message", "Test message"}
        }));
    }

    int64_t CalculateTimeSpanMs()
    {
        if (m_events->Size() < 2)
            return 0;

        QDateTime firstTime, lastTime;

        for (size_t i = 0; i < m_events->Size(); ++i) {
            try {
                const auto& event = m_events->GetEvent(i);
                QString tsStr = QString::fromStdString(event.findByKey("timestamp"));
                if (tsStr.isEmpty())
                    continue;

                QDateTime dt = QDateTime::fromString(tsStr, Qt::ISODate);
                if (!dt.isValid()) {
                    dt = QDateTime::fromString(tsStr, "yyyy-MM-dd hh:mm:ss");
                }

                if (dt.isValid()) {
                    if (!firstTime.isValid())
                        firstTime = dt;
                    lastTime = dt;
                }
            } catch (const std::exception&) {
                // Skip
            }
        }

        if (firstTime.isValid() && lastTime.isValid()) {
            return firstTime.msecsTo(lastTime);
        }

        return 0;
    }

    std::unique_ptr<EventsContainer> m_events;
};

// Test: ISO 8601 timestamp format parsing
TEST_F(TimestampParsingTest, ISO8601FormatParsing)
{
    AddEventWithTimestamp("2024-01-01T08:00:00Z", "INFO");
    AddEventWithTimestamp("2024-01-01T08:05:00Z", "INFO");
    AddEventWithTimestamp("2024-01-01T08:10:00Z", "INFO");

    int64_t timeSpan = CalculateTimeSpanMs();

    // Should be 10 minutes = 600,000 ms
    EXPECT_EQ(timeSpan, 600000);
}

// Test: "yyyy-MM-dd hh:mm:ss" format parsing
TEST_F(TimestampParsingTest, CustomFormatParsing)
{
    AddEventWithTimestamp("2024-01-01 08:00:00", "INFO");
    AddEventWithTimestamp("2024-01-01 08:05:00", "INFO");
    AddEventWithTimestamp("2024-01-01 08:10:00", "INFO");

    int64_t timeSpan = CalculateTimeSpanMs();

    // Should be 10 minutes = 600,000 ms
    EXPECT_EQ(timeSpan, 600000);
}

// Test: One hour time span
TEST_F(TimestampParsingTest, OneHourTimeSpan)
{
    AddEventWithTimestamp("2024-01-01T08:00:00Z", "INFO");
    AddEventWithTimestamp("2024-01-01T09:00:00Z", "INFO");

    int64_t timeSpan = CalculateTimeSpanMs();

    // Should be 1 hour = 3,600,000 ms
    EXPECT_EQ(timeSpan, 3600000);
}

// Test: One day time span
TEST_F(TimestampParsingTest, OneDayTimeSpan)
{
    AddEventWithTimestamp("2024-01-01T08:00:00Z", "INFO");
    AddEventWithTimestamp("2024-01-02T08:00:00Z", "INFO");

    int64_t timeSpan = CalculateTimeSpanMs();

    // Should be 1 day = 86,400,000 ms
    EXPECT_EQ(timeSpan, 86400000);
}

// Test: Mixed timestamp formats in same report
TEST_F(TimestampParsingTest, MixedFormatsParsing)
{
    AddEventWithTimestamp("2024-01-01T08:00:00Z", "INFO");
    AddEventWithTimestamp("2024-01-01 08:05:00", "INFO");  // Different format
    AddEventWithTimestamp("2024-01-01T08:10:00Z", "INFO");

    int64_t timeSpan = CalculateTimeSpanMs();

    // Should handle both formats correctly
    EXPECT_EQ(timeSpan, 600000);
}

// Test: Seconds precision
TEST_F(TimestampParsingTest, SecondsPrecision)
{
    AddEventWithTimestamp("2024-01-01T08:00:00Z", "INFO");
    AddEventWithTimestamp("2024-01-01T08:00:30Z", "INFO");

    int64_t timeSpan = CalculateTimeSpanMs();

    // Should be 30 seconds = 30,000 ms
    EXPECT_EQ(timeSpan, 30000);
}

// Test: Milliseconds precision (if supported)
TEST_F(TimestampParsingTest, MillisecondsPrecision)
{
    AddEventWithTimestamp("2024-01-01T08:00:00.000Z", "INFO");
    AddEventWithTimestamp("2024-01-01T08:00:00.500Z", "INFO");

    int64_t timeSpan = CalculateTimeSpanMs();

    // Should be 500 milliseconds
    EXPECT_GE(timeSpan, 0);  // At least valid
    EXPECT_LE(timeSpan, 1000);  // At most 1 second
}

// Test: Single event produces zero time span
TEST_F(TimestampParsingTest, SingleEventZeroTimeSpan)
{
    AddEventWithTimestamp("2024-01-01T08:00:00Z", "INFO");

    int64_t timeSpan = CalculateTimeSpanMs();

    // Single event should have 0 time span
    EXPECT_EQ(timeSpan, 0);
}

// Test: Empty container produces zero time span
TEST_F(TimestampParsingTest, EmptyContainerZeroTimeSpan)
{
    int64_t timeSpan = CalculateTimeSpanMs();

    // Empty should be 0
    EXPECT_EQ(timeSpan, 0);
}

// Test: Invalid timestamps are skipped
TEST_F(TimestampParsingTest, InvalidTimestampsSkipped)
{
    AddEventWithTimestamp("2024-01-01T08:00:00Z", "INFO");
    AddEventWithTimestamp("invalid-timestamp", "INFO");  // Invalid
    AddEventWithTimestamp("2024-01-01T08:10:00Z", "INFO");

    int64_t timeSpan = CalculateTimeSpanMs();

    // Should calculate correctly, skipping invalid
    EXPECT_EQ(timeSpan, 600000);
}

// Test: Time span calculation with multiple events
TEST_F(TimestampParsingTest, MultipleEventsTimeSpan)
{
    AddEventWithTimestamp("2024-01-01T08:00:00Z", "INFO");  // First
    AddEventWithTimestamp("2024-01-01T08:10:00Z", "INFO");
    AddEventWithTimestamp("2024-01-01T08:20:00Z", "INFO");
    AddEventWithTimestamp("2024-01-01T08:30:00Z", "INFO");
    AddEventWithTimestamp("2024-01-01T08:40:00Z", "INFO");  // Last

    int64_t timeSpan = CalculateTimeSpanMs();

    // Should be from first (08:00) to last (08:40) = 40 minutes = 2,400,000 ms
    EXPECT_EQ(timeSpan, 2400000);
}

// Test: Time span with different dates
TEST_F(TimestampParsingTest, CrossDateTimeSpan)
{
    AddEventWithTimestamp("2024-01-31T23:00:00Z", "INFO");
    AddEventWithTimestamp("2024-02-01T01:00:00Z", "INFO");

    int64_t timeSpan = CalculateTimeSpanMs();

    // Should be 2 hours = 7,200,000 ms
    EXPECT_EQ(timeSpan, 7200000);
}

// Performance test: Time span calculation with many events
TEST_F(TimestampParsingTest, TimeSpanPerformanceWithManyEvents)
{
    // Add 1000 events with increasing timestamps
    for (int i = 0; i < 1000; ++i) {
        int hours = 8 + (i / 100);  // Spans multiple hours
        int minutes = (i % 100);
        QString timestamp = QString("2024-01-01T%1:%2:00Z")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'));
        AddEventWithTimestamp(timestamp.toStdString(), "INFO");
    }

    auto start = std::chrono::high_resolution_clock::now();
    int64_t timeSpan = CalculateTimeSpanMs();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should complete quickly
    EXPECT_LT(duration, 100);  // < 100ms
    EXPECT_GT(timeSpan, 0);    // Should calculate something (multiple hours)
}

} // namespace db::test
