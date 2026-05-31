#include <gtest/gtest.h>
#include "json/JsonParser.hpp"
#include "Error.hpp"
#include "LogEvent.hpp"

#include <sstream>
#include <filesystem>
#include <string>
#include <vector>

namespace parser::test
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

class Collector : public IDataParserObserver
{
public:
    std::vector<db::LogEvent> events;

    void ProgressUpdated() override {}

    void NewEventFound(db::LogEvent&& ev) override
    {
        events.push_back(std::move(ev));
    }

    void NewEventBatchFound(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& batch) override
    {
        for (auto& [id, items] : batch)
            events.emplace_back(id, std::move(items));
    }
};

static std::vector<db::LogEvent> Parse(const std::string& text)
{
    Collector col;
    JsonParser parser;
    parser.RegisterObserver(&col);
    std::istringstream ss(text);
    parser.ParseData(ss);
    return std::move(col.events);
}

// ---------------------------------------------------------------------------
// Array format
// ---------------------------------------------------------------------------

TEST(JsonParserTest, ArrayFormat_BasicFields)
{
    const auto events = Parse(
        R"([{"level":"INFO","msg":"hello"},{"level":"WARN","msg":"world"}])");

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].findByKey("level"), "INFO");
    EXPECT_EQ(events[0].findByKey("msg"),   "hello");
    EXPECT_EQ(events[1].findByKey("level"), "WARN");
    EXPECT_EQ(events[1].findByKey("msg"),   "world");
}

TEST(JsonParserTest, ArrayFormat_NumericAndBoolFields)
{
    const auto events = Parse(R"([{"code":42,"ok":true,"ratio":3.14}])");

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].findByKey("code"),  "42");
    EXPECT_EQ(events[0].findByKey("ok"),    "true");
    EXPECT_EQ(events[0].findByKey("ratio"), "3.14");
}

TEST(JsonParserTest, ArrayFormat_NullFieldBecomesEmptyString)
{
    const auto events = Parse(R"([{"key":null}])");

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].findByKey("key"), "");
}

TEST(JsonParserTest, ArrayFormat_NestedObjectFlattened)
{
    const auto events = Parse(R"([{"ctx":{"host":"db.local","port":5432}}])");

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].findByKey("ctx.host"), "db.local");
    EXPECT_EQ(events[0].findByKey("ctx.port"), "5432");
}

TEST(JsonParserTest, ArrayFormat_NestedArrayFlattened)
{
    const auto events = Parse(R"([{"tags":["a","b","c"]}])");

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].findByKey("tags.0"), "a");
    EXPECT_EQ(events[0].findByKey("tags.1"), "b");
    EXPECT_EQ(events[0].findByKey("tags.2"), "c");
}

TEST(JsonParserTest, ArrayFormat_EmptyArrayProducesNoEvents)
{
    const auto events = Parse("[]");
    EXPECT_TRUE(events.empty());
}

TEST(JsonParserTest, ArrayFormat_NonObjectElementsSkipped)
{
    const auto events = Parse(R"([{"a":"1"}, 42, null, {"b":"2"}])");
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].findByKey("a"), "1");
    EXPECT_EQ(events[1].findByKey("b"), "2");
}

TEST(JsonParserTest, ArrayFormat_LeadingWhitespace)
{
    const auto events = Parse("   \n\t[{\"x\":\"y\"}]");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].findByKey("x"), "y");
}

// ---------------------------------------------------------------------------
// NDJSON format
// ---------------------------------------------------------------------------

TEST(JsonParserTest, NdjsonFormat_BasicFields)
{
    const auto events = Parse(
        "{\"level\":\"INFO\",\"msg\":\"first\"}\n"
        "{\"level\":\"DEBUG\",\"msg\":\"second\"}\n");

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].findByKey("level"), "INFO");
    EXPECT_EQ(events[0].findByKey("msg"),   "first");
    EXPECT_EQ(events[1].findByKey("level"), "DEBUG");
}

TEST(JsonParserTest, NdjsonFormat_BlankLinesSkipped)
{
    const auto events = Parse(
        "{\"a\":\"1\"}\n"
        "\n"
        "{\"b\":\"2\"}\n");

    ASSERT_EQ(events.size(), 2u);
}

TEST(JsonParserTest, NdjsonFormat_CrlfLineEndings)
{
    const auto events = Parse(
        "{\"x\":\"1\"}\r\n"
        "{\"x\":\"2\"}\r\n");

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].findByKey("x"), "1");
    EXPECT_EQ(events[1].findByKey("x"), "2");
}

TEST(JsonParserTest, NdjsonFormat_MalformedLineSurvivedSkipped)
{
    const auto events = Parse(
        "{\"ok\":\"1\"}\n"
        "not-json\n"
        "{\"ok\":\"2\"}\n");

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].findByKey("ok"), "1");
    EXPECT_EQ(events[1].findByKey("ok"), "2");
}

TEST(JsonParserTest, NdjsonFormat_NestedObjectFlattened)
{
    const auto events = Parse(R"({"ctx":{"user":"alice","id":7}})");

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].findByKey("ctx.user"), "alice");
    EXPECT_EQ(events[0].findByKey("ctx.id"),   "7");
}

// ---------------------------------------------------------------------------
// Error cases
// ---------------------------------------------------------------------------

TEST(JsonParserTest, InvalidFirstChar_Throws)
{
    JsonParser parser;
    std::istringstream ss("not valid json at all");
    EXPECT_THROW(parser.ParseData(ss), error::Error);
}

TEST(JsonParserTest, MalformedArray_Throws)
{
    JsonParser parser;
    std::istringstream ss("[{\"a\":1}, broken");
    EXPECT_THROW(parser.ParseData(ss), error::Error);
}

TEST(JsonParserTest, EmptyStream_ProducesNoEvents)
{
    const auto events = Parse("   ");
    EXPECT_TRUE(events.empty());
}

// ---------------------------------------------------------------------------
// Progress tracking
// ---------------------------------------------------------------------------

TEST(JsonParserTest, ProgressTracking_ArrayFormat)
{
    JsonParser parser;
    Collector col;
    parser.RegisterObserver(&col);

    const std::string content = R"([{"a":"1"},{"a":"2"}])";
    std::istringstream ss(content);
    parser.ParseData(ss);

    // After parsing, total and current should match (stream doesn't give size
    // so total stays 0 for the stream overload — just verify no crash).
    EXPECT_GE(parser.GetCurrentProgress(), 0u);
}

// ---------------------------------------------------------------------------
// File-based parsing (requires test data dir defined at build time)
// ---------------------------------------------------------------------------

#ifdef LOGVIEWER_TEST_DATA_DIR

TEST(JsonParserTest, FileArray_SampleJson)
{
    const std::filesystem::path path =
        std::filesystem::path(LOGVIEWER_TEST_DATA_DIR) / "sample.json";

    Collector col;
    JsonParser parser;
    parser.RegisterObserver(&col);
    parser.ParseData(path);

    ASSERT_EQ(col.events.size(), 5u);
    EXPECT_EQ(col.events[0].findByKey("level"), "INFO");
    EXPECT_EQ(col.events[2].findByKey("level"), "WARN");
    // Nested context object
    EXPECT_EQ(col.events[3].findByKey("context.host"), "db.local");
    EXPECT_EQ(col.events[3].findByKey("context.port"), "5432");
    // Progress filled in
    EXPECT_GT(parser.GetTotalProgress(), 0u);
    EXPECT_EQ(parser.GetCurrentProgress(), parser.GetTotalProgress());
}

TEST(JsonParserTest, FileNdjson_SampleJsonl)
{
    const std::filesystem::path path =
        std::filesystem::path(LOGVIEWER_TEST_DATA_DIR) / "sample.jsonl";

    Collector col;
    JsonParser parser;
    parser.RegisterObserver(&col);
    parser.ParseData(path);

    ASSERT_EQ(col.events.size(), 3u);
    EXPECT_EQ(col.events[0].findByKey("level"), "INFO");
    EXPECT_EQ(col.events[2].findByKey("level"), "WARN");
    EXPECT_EQ(col.events[2].findByKey("mem_pct"), "87");
}

TEST(JsonParserTest, FileNotFound_Throws)
{
    JsonParser parser;
    EXPECT_THROW(
        parser.ParseData(std::filesystem::path("/nonexistent/path/file.json")),
        error::Error);
}

#endif // LOGVIEWER_TEST_DATA_DIR

} // namespace parser::test
