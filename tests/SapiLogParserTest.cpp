#include <gtest/gtest.h>
#include "sapi/SapiLogParser.hpp"
#include "ParserFactory.hpp"
#include "Error.hpp"
#include "LogEvent.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace parser::test
{

namespace
{

// Anonymous namespace: other parser tests define their own `Collector` in
// parser::test — a shared name with external linkage would be an ODR clash.
class SapiCollector : public IDataParserObserver
{
public:
    std::vector<db::LogEvent> events;

    void ProgressUpdated() override {}

    void NewEventFound(db::LogEvent&& ev) override { events.push_back(std::move(ev)); }

    void NewEventBatchFound(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& batch) override
    {
        for (auto& [id, items] : batch)
            events.emplace_back(id, std::move(items));
    }
};

std::vector<db::LogEvent> Parse(const std::string& text)
{
    SapiCollector col;
    SapiLogParser parser;
    parser.RegisterObserver(&col);
    std::istringstream ss(text);
    parser.ParseData(ss);
    return std::move(col.events);
}

constexpr const char* kTs = "[2026-09-19T06:46:46.456779Z]";

std::string Line(const std::string& rest) { return std::string(kTs) + " " + rest + "\n"; }

} // namespace

// ---------------------------------------------------------------------------
// ParseLine
// ---------------------------------------------------------------------------

TEST(SapiLogParserTest, ParsesAllEightFields)
{
    db::LogEvent::EventItems items;
    ASSERT_TRUE(SapiLogParser::ParseLine(
        "[2026-09-19T06:47:18.404422Z] [IO] [robot] [il] [INFO] [sendMessage] "
        "[Control command sent] [{\"cmd\": \"sendMessage\"}]", items));

    const db::LogEvent ev(1, std::move(items));
    EXPECT_EQ(ev.findByKey("timestamp"),   "2026-09-19T06:47:18.404422Z");
    EXPECT_EQ(ev.findByKey("category"),    "IO");
    EXPECT_EQ(ev.findByKey("source"),      "robot");
    EXPECT_EQ(ev.findByKey("destination"), "il");
    EXPECT_EQ(ev.findByKey("level"),       "INFO");
    EXPECT_EQ(ev.findByKey("event_type"),  "sendMessage");
    EXPECT_EQ(ev.findByKey("info"),        "Control command sent");
    EXPECT_EQ(ev.findByKey("payload"),     "{\"cmd\": \"sendMessage\"}");
}

TEST(SapiLogParserTest, NestedBracketsInInfoAreKept)
{
    const auto events = Parse(Line(
        "[GENERAL] [c-il-east] [internal] [INFO] [LOG] [Log message] [[ 419310003] [GW-IL EAST] starting]"));
    // Timestamp is the first bracket group: Line() prepends it.
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].findByKey("info"), "Log message");
    EXPECT_EQ(events[0].findByKey("payload"), "[ 419310003] [GW-IL EAST] starting");
}

TEST(SapiLogParserTest, MultipleTrailingGroupsKeptVerbatimAsPayload)
{
    const auto events = Parse(Line(
        "[IO] [train-multi] [c-west,c-east] [INFO] [M136] [ERTMS position report] "
        "[train_id=1] [P0] [cycle=1 v_train=0]"));
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].findByKey("destination"), "c-west,c-east");
    EXPECT_EQ(events[0].findByKey("payload"), "[train_id=1] [P0] [cycle=1 v_train=0]");
}

TEST(SapiLogParserTest, EmptyPayloadIsOmitted)
{
    const auto events = Parse(Line(
        "[GENERAL] [ctc] [internal] [INFO] [INIT] [CTC sim starting] []"));
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].findByKey("info"), "CTC sim starting");
    EXPECT_TRUE(events[0].findAllByKey("payload").empty());
}

TEST(SapiLogParserTest, ErrorAndFailLevelsPreserved)
{
    const auto events = Parse(
        Line("[GENERAL] [robot] [internal] [FAIL] [LOG] [Log message] [boom]") +
        Line("[IO] [robot] [internal] [ERROR] [STEP_FAIL] [Verify] [boom]"));
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].findByKey("level"), "FAIL");
    EXPECT_EQ(events[1].findByKey("level"), "ERROR");
}

// ---------------------------------------------------------------------------
// Stream handling
// ---------------------------------------------------------------------------

TEST(SapiLogParserTest, HeaderBlankAndMalformedLinesAreSkipped)
{
    const auto events = Parse(
        "# FULL MERGED TEST STEPS, CONTAINER & SIMULATOR LOGS\n"
        "# Status      : FAIL\n"
        "\n" +
        Line("[GENERAL] [a-west] [internal] [INFO] [LOG] [Log message] [ok]") +
        "not a log line at all\n"
        "[unterminated bracket line\n" +
        Line("[GENERAL] [b-west] [internal] [DEBUG] [LOG] [Log message] [ok2]"));

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].findByKey("source"), "a-west");
    EXPECT_EQ(events[1].findByKey("source"), "b-west");
}

TEST(SapiLogParserTest, CrlfLineEndingsHandled)
{
    const auto events = Parse(
        std::string(kTs) + " [GENERAL] [a] [internal] [INFO] [LOG] [Log message] [x]\r\n");
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].findByKey("payload"), "x");
}

TEST(SapiLogParserTest, IdsAreSequentialFromOne)
{
    const auto events = Parse(
        Line("[GENERAL] [a] [internal] [INFO] [LOG] [Log message] [1]") +
        Line("[GENERAL] [a] [internal] [INFO] [LOG] [Log message] [2]"));
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].getId(), 1);
    EXPECT_EQ(events[1].getId(), 2);
}

TEST(SapiLogParserTest, NoMatchingLineThrows)
{
    EXPECT_THROW(Parse("<events><event/></events>\nplain text\n"), error::Error);
}

TEST(SapiLogParserTest, EmptyInputYieldsNoEvents)
{
    EXPECT_TRUE(Parse("").empty());
}

// ---------------------------------------------------------------------------
// File-based: sample data, sniffing and factory dispatch
// ---------------------------------------------------------------------------

#ifdef LOGVIEWER_TEST_DATA_DIR

TEST(SapiLogParserTest, SampleFileParsesAllDataLines)
{
    const auto path = std::filesystem::path(LOGVIEWER_TEST_DATA_DIR) / "sapi_sample.txt";
    SapiCollector col;
    SapiLogParser parser;
    parser.RegisterObserver(&col);
    parser.ParseData(path);

    ASSERT_EQ(col.events.size(), 12u);
    EXPECT_EQ(col.events[0].findByKey("source"), "c-il-east");
    EXPECT_EQ(parser.GetCurrentProgress(), parser.GetTotalProgress());

    const auto& multi = col.events[8];
    EXPECT_EQ(multi.findByKey("event_type"), "M136");
    EXPECT_EQ(multi.findByKey("payload").substr(0, 12), "[train_id=1]");
    EXPECT_EQ(col.events[9].findByKey("level"), "FAIL");
}

TEST(SapiLogParserTest, LooksLikeSapiLogDetectsHeader)
{
    const auto path = std::filesystem::path(LOGVIEWER_TEST_DATA_DIR) / "sapi_sample.txt";
    EXPECT_TRUE(SapiLogParser::LooksLikeSapiLog(path));
    EXPECT_FALSE(SapiLogParser::LooksLikeSapiLog(
        std::filesystem::path(LOGVIEWER_TEST_DATA_DIR) / "sample.json"));
    EXPECT_FALSE(SapiLogParser::LooksLikeSapiLog("/nonexistent/file.txt"));
}

TEST(SapiLogParserTest, FactoryPicksSapiParserForTxtWithHeader)
{
    const auto path = std::filesystem::path(LOGVIEWER_TEST_DATA_DIR) / "sapi_sample.txt";
    auto result = ParserFactory::CreateFromFile(path);
    ASSERT_TRUE(result.isOk());
    auto created = result.unwrap();
    EXPECT_NE(dynamic_cast<SapiLogParser*>(created.get()), nullptr);
}

TEST(SapiLogParserTest, FactoryKeepsFallbackForOtherTxtFiles)
{
    const auto path = std::filesystem::temp_directory_path() / "sapi_test_plain.txt";
    { std::ofstream(path) << "just some text\n"; }
    auto result = ParserFactory::CreateFromFile(path);
    ASSERT_TRUE(result.isOk());
    auto created = result.unwrap();
    EXPECT_EQ(dynamic_cast<SapiLogParser*>(created.get()), nullptr);
    std::filesystem::remove(path);
}

#endif // LOGVIEWER_TEST_DATA_DIR

} // namespace parser::test
