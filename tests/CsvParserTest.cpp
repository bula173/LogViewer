/**
 * @file CsvParserTest.cpp
 * @brief Unit tests for CSV parser
 * @author LogViewer Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include "csv/CsvParser.hpp"
#include "LogEvent.hpp"
#include <sstream>
#include <vector>

namespace parser
{
namespace test
{

/**
 * @brief Test observer for capturing parsed events
 */
class TestObserver : public IDataParserObserver
{
  public:
    std::vector<db::LogEvent> events;
    int progressUpdateCount = 0;

    void ProgressUpdated() override
    {
        progressUpdateCount++;
    }

    void NewEventFound(db::LogEvent&& event) override
    {
        events.push_back(std::move(event));
    }

    void NewEventBatchFound(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& eventBatch) override
    {
        for (auto& [id, items] : eventBatch)
        {
            events.emplace_back(id, std::move(items));
        }
    }
};

/**
 * @brief Test basic CSV parsing with headers
 */
TEST(CsvParserTest, ParseSimpleCSV)
{
    std::stringstream input;
    input << "id,timestamp,level,info\n";
    input << "1,2025-12-07 10:00:00,INFO,Test message 1\n";
    input << "2,2025-12-07 10:00:01,DEBUG,Test message 2\n";
    input << "3,2025-12-07 10:00:02,ERROR,Test message 3\n";

    CsvParser parser;
    TestObserver observer;
    parser.RegisterObserver(&observer);

    EXPECT_NO_THROW(parser.ParseData(input));
    EXPECT_EQ(observer.events.size(), 3);
}

/**
 * @brief Test CSV parsing with quoted fields
 */
TEST(CsvParserTest, ParseQuotedFields)
{
    std::stringstream input;
    input << "id,timestamp,level,info\n";
    input << "1,2025-12-07 10:00:00,INFO,\"Message with, comma\"\n";
    input << "2,2025-12-07 10:00:01,DEBUG,\"Message with \"\"quotes\"\"\"\n";

    CsvParser parser;
    TestObserver observer;
    parser.RegisterObserver(&observer);

    EXPECT_NO_THROW(parser.ParseData(input));
    EXPECT_EQ(observer.events.size(), 2);
}

/**
 * @brief Test CSV parsing with custom delimiter
 */
TEST(CsvParserTest, ParseCustomDelimiter)
{
    std::stringstream input;
    input << "id;timestamp;level;info\n";
    input << "1;2025-12-07 10:00:00;INFO;Test message 1\n";
    input << "2;2025-12-07 10:00:01;DEBUG;Test message 2\n";

    CsvParser parser;
    parser.SetDelimiter(';');
    TestObserver observer;
    parser.RegisterObserver(&observer);

    EXPECT_NO_THROW(parser.ParseData(input));
    EXPECT_EQ(observer.events.size(), 2);
}

/**
 * @brief Test CSV parsing without headers
 */
TEST(CsvParserTest, ParseWithoutHeaders)
{
    std::stringstream input;
    input << "1,2025-12-07 10:00:00,INFO,Test message 1\n";
    input << "2,2025-12-07 10:00:01,DEBUG,Test message 2\n";

    CsvParser parser;
    parser.SetHasHeaders(false);
    TestObserver observer;
    parser.RegisterObserver(&observer);

    EXPECT_NO_THROW(parser.ParseData(input));
    EXPECT_EQ(observer.events.size(), 2);
}

/**
 * @brief Test CSV parsing with empty lines
 */
TEST(CsvParserTest, ParseWithEmptyLines)
{
    std::stringstream input;
    input << "id,timestamp,level,info\n";
    input << "1,2025-12-07 10:00:00,INFO,Test message 1\n";
    input << "\n";
    input << "2,2025-12-07 10:00:01,DEBUG,Test message 2\n";
    input << "   \n";
    input << "3,2025-12-07 10:00:02,ERROR,Test message 3\n";

    CsvParser parser;
    TestObserver observer;
    parser.RegisterObserver(&observer);

    EXPECT_NO_THROW(parser.ParseData(input));
    EXPECT_EQ(observer.events.size(), 3);
}

/**
 * @brief Test CSV parsing with various field mappings
 */
TEST(CsvParserTest, ParseVariousFieldNames)
{
    std::stringstream input;
    input << "id,time,severity,message,source,threadid\n";
    input << "1,2025-12-07 10:00:00,INFO,Test message,TestModule,123\n";

    CsvParser parser;
    TestObserver observer;
    parser.RegisterObserver(&observer);

    EXPECT_NO_THROW(parser.ParseData(input));
    EXPECT_EQ(observer.events.size(), 1);
}

/**
 * @brief Test progress tracking
 */
TEST(CsvParserTest, ProgressTracking)
{
    std::stringstream input;
    input << "id,timestamp,level,info\n";
    for (int i = 1; i <= 100; ++i)
    {
        input << i << ",2025-12-07 10:00:00,INFO,Test message " << i << "\n";
    }

    CsvParser parser;
    TestObserver observer;
    parser.RegisterObserver(&observer);

    parser.ParseData(input);

    EXPECT_GT(observer.progressUpdateCount, 0);
    EXPECT_EQ(parser.GetCurrentProgress(), parser.GetTotalProgress());
}

/**
 * @brief CRLF line endings must not hang the parser (trailing '\r' after an
 *        unquoted field, an empty field, or a quoted field).
 */
TEST(CsvParserTest, ParseCrlfLineEndings)
{
    std::stringstream input;
    input << "id,level,info\r\n";
    input << "1,INFO,plain\r\n";
    input << "2,WARN,\r\n";
    input << "3,ERROR,\"quoted, text\"\r\n";

    CsvParser parser;
    TestObserver observer;
    parser.RegisterObserver(&observer);

    EXPECT_NO_THROW(parser.ParseData(input));
    ASSERT_EQ(observer.events.size(), 3u);
    EXPECT_EQ(observer.events[0].findByKey("info"), "plain");
    EXPECT_EQ(observer.events[1].findByKey("level"), "WARN");
    EXPECT_EQ(observer.events[2].findByKey("info"), "quoted, text");
}

/**
 * @brief A UTF-8 BOM must not become part of the first header name.
 */
TEST(CsvParserTest, ParseUtf8BomHeader)
{
    std::stringstream input;
    input << "\xEF\xBB\xBFid,level,info\n";
    input << "7,INFO,hello\n";

    CsvParser parser;
    TestObserver observer;
    parser.RegisterObserver(&observer);

    EXPECT_NO_THROW(parser.ParseData(input));
    ASSERT_EQ(observer.events.size(), 1u);
    EXPECT_EQ(observer.events[0].findByKey("id"), "7");
    EXPECT_EQ(observer.events[0].findByKey("info"), "hello");
}

/**
 * @brief Leading blank lines must not cause the header row to be read as data.
 */
TEST(CsvParserTest, HeaderAfterLeadingBlankLines)
{
    std::stringstream input;
    input << "\n   \n";
    input << "id,level,info\n";
    input << "1,INFO,first\n";

    CsvParser parser;
    TestObserver observer;
    parser.RegisterObserver(&observer);

    EXPECT_NO_THROW(parser.ParseData(input));
    ASSERT_EQ(observer.events.size(), 1u);
    EXPECT_EQ(observer.events[0].findByKey("info"), "first");
}

} // namespace test
} // namespace parser
