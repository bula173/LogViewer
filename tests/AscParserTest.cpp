#include <gtest/gtest.h>

#include "asc/AscParser.hpp"
#include "dbc/DbcParser.hpp"
#include "IDataParser.hpp"
#include "LogEvent.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace parser
{
namespace test
{

// ---------------------------------------------------------------------------
// Shared test observer — collects all events from batch and single callbacks.
// ---------------------------------------------------------------------------
class AscTestObserver : public IDataParserObserver
{
  public:
    std::vector<db::LogEvent> events;
    int progressCount = 0;

    void ProgressUpdated() override { ++progressCount; }

    void NewEventFound(db::LogEvent&& event) override
    {
        events.push_back(std::move(event));
    }

    void NewEventBatchFound(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& batch) override
    {
        for (auto& [id, items] : batch)
            events.emplace_back(id, std::move(items));
    }

    // Find value of a field by key in the Nth event (0-based).
    std::string Field(size_t eventIdx, const std::string& key) const
    {
        if (eventIdx >= events.size()) return {};
        return events[eventIdx].findByKey(key);
    }
};

// ---------------------------------------------------------------------------
// Minimal ASC content used by multiple tests.
// ---------------------------------------------------------------------------
static const char* kSimpleAsc =
    "date Thu May 21 10:00:00 2026\n"
    "base hex  timestamps absolute\n"
    "Begin measurement\n"
    "   0.000100 1 001             Rx   d  8  00 80 00 64 00 00 00 00\n"
    "   0.001000 1 7FF             Tx   d  3  E8 03 00\n"
    "   0.002000 1 18DAF110X       Rx   d  8  02 01 0D 00 00 00 00 00\n"
    "   0.003000 1 ErrorFrame\n"
    "   0.004000 2 100             Rx   d  4  AA BB CC DD\n"
    "End measurement\n";

// ---------------------------------------------------------------------------
// Basic parsing
// ---------------------------------------------------------------------------

TEST(AscParserTest, ParseEventCount)
{
    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(kSimpleAsc);
    EXPECT_NO_THROW(parser.ParseData(ss));
    EXPECT_EQ(obs.events.size(), 5u);
}

TEST(AscParserTest, StandardFrameFields)
{
    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(kSimpleAsc);
    parser.ParseData(ss);

    // Event 0: standard Rx frame 0x001
    EXPECT_EQ(obs.Field(0, "timestamp"),   "0.000100");
    EXPECT_EQ(obs.Field(0, "type"),        "Rx");
    EXPECT_EQ(obs.Field(0, "CAN_Channel"), "1");
    EXPECT_EQ(obs.Field(0, "CAN_ID"),      "001");
    EXPECT_EQ(obs.Field(0, "CAN_IDE"),     "Standard");
    EXPECT_EQ(obs.Field(0, "CAN_DLC"),     "8");
    EXPECT_EQ(obs.Field(0, "CAN_Data"),    "00 80 00 64 00 00 00 00");
}

TEST(AscParserTest, TxFrameDirection)
{
    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(kSimpleAsc);
    parser.ParseData(ss);

    EXPECT_EQ(obs.Field(1, "type"),   "Tx");
    EXPECT_EQ(obs.Field(1, "CAN_ID"), "7FF");
    EXPECT_EQ(obs.Field(1, "CAN_DLC"), "3");
    EXPECT_EQ(obs.Field(1, "CAN_Data"), "E8 03 00");
}

TEST(AscParserTest, ExtendedFrame)
{
    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(kSimpleAsc);
    parser.ParseData(ss);

    // Event 2: extended frame 18DAF110X
    EXPECT_EQ(obs.Field(2, "CAN_IDE"), "Extended");
    EXPECT_EQ(obs.Field(2, "CAN_ID"),  "18DAF110");
}

TEST(AscParserTest, ErrorFrame)
{
    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(kSimpleAsc);
    parser.ParseData(ss);

    // Event 3: ErrorFrame
    EXPECT_EQ(obs.Field(3, "type"), "ErrorFrame");
    EXPECT_EQ(obs.Field(3, "CAN_Channel"), "1");
}

TEST(AscParserTest, Channel2Frame)
{
    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(kSimpleAsc);
    parser.ParseData(ss);

    EXPECT_EQ(obs.Field(4, "CAN_Channel"), "2");
    EXPECT_EQ(obs.Field(4, "CAN_ID"),      "100");
    EXPECT_EQ(obs.Field(4, "CAN_Data"),    "AA BB CC DD");
}

// ---------------------------------------------------------------------------
// Comment and header lines are skipped
// ---------------------------------------------------------------------------

TEST(AscParserTest, SkipsCommentAndHeaderLines)
{
    const char* input =
        "// This is a comment\n"
        "date Thu May 21 2026\n"
        "base hex  timestamps absolute\n"
        "Begin measurement\n"
        "// Another comment\n"
        "   1.000000 1 0AB  Rx   d  1  FF\n"
        "End measurement\n";

    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(input);
    parser.ParseData(ss);

    EXPECT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "CAN_ID"), "0AB");
}

// ---------------------------------------------------------------------------
// DBC signal decoding
// ---------------------------------------------------------------------------

static const char* kAscWithEngineFrame =
    "Begin measurement\n"
    // ID 1 = EngineStatus: RPM at bits 0-15 Intel, factor 0.5
    // byte[0..1] = 0x10 0x80 → raw=0x8010=32784 → RPM=16392
    // byte[2]    = 0x00 → Throttle raw=0 → 0%
    // byte[3]    = 0x64 → EngineTemp raw=100 → 60°C
    "   0.001000 1 001 Rx d 8 10 80 00 64 00 00 00 00\n"
    "End measurement\n";

TEST(AscParserTest, DbcSignalDecoding_MsgNamePresent)
{
    // Build a DBC in memory by writing a temp file would be complex; instead
    // use ParseDbcFile on the example file if available, otherwise skip.
    // We test via the string-stream path using a manually constructed DbcDatabase
    // injected through the DbcParser header directly.
    //
    // Since AscParser loads DBC from a file path, we test with the example file
    // that was copied to the test binary directory.

    const std::filesystem::path dbcPath =
        std::filesystem::path(LOGVIEWER_TEST_DATA_DIR) / "example.dbc";

    if (!std::filesystem::exists(dbcPath))
        GTEST_SKIP() << "example.dbc not found at " << dbcPath;

    AscParser parser(dbcPath);
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(kAscWithEngineFrame);
    parser.ParseData(ss);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "CAN_MsgName"), "EngineStatus");
    // SIG:RPM should be present
    EXPECT_FALSE(obs.Field(0, "SIG:RPM").empty());
}

TEST(AscParserTest, DbcSignalDecoding_UnknownIdNoMsgName)
{
    const std::filesystem::path dbcPath =
        std::filesystem::path(LOGVIEWER_TEST_DATA_DIR) / "example.dbc";

    if (!std::filesystem::exists(dbcPath))
        GTEST_SKIP() << "example.dbc not found at " << dbcPath;

    AscParser parser(dbcPath);
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    // ID 0x999 is not in the DBC
    const char* input = "Begin measurement\n   0.001 1 999 Rx d 4 AA BB CC DD\nEnd measurement\n";
    std::istringstream ss(input);
    parser.ParseData(ss);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_TRUE(obs.Field(0, "CAN_MsgName").empty());
    EXPECT_TRUE(obs.Field(0, "SIG:RPM").empty());
}

// ---------------------------------------------------------------------------
// Empty / whitespace-only input
// ---------------------------------------------------------------------------

TEST(AscParserTest, EmptyInput)
{
    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss("");
    EXPECT_NO_THROW(parser.ParseData(ss));
    EXPECT_EQ(obs.events.size(), 0u);
}

TEST(AscParserTest, OnlyHeaderLines)
{
    const char* input =
        "date Thu May 21 2026\n"
        "base hex  timestamps absolute\n"
        "Begin measurement\n"
        "End measurement\n";

    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(input);
    EXPECT_NO_THROW(parser.ParseData(ss));
    EXPECT_EQ(obs.events.size(), 0u);
}

// ---------------------------------------------------------------------------
// Progress tracking
// ---------------------------------------------------------------------------

TEST(AscParserTest, ProgressIsUpdated)
{
    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(kSimpleAsc);
    parser.ParseData(ss);

    // At least one progress notification expected per batch flush.
    EXPECT_GE(obs.progressCount, 1);
}

// ---------------------------------------------------------------------------
// Data upper-case hex normalisation
// ---------------------------------------------------------------------------

TEST(AscParserTest, DataBytesUpperCase)
{
    const char* input =
        "Begin measurement\n"
        "   0.001 1 0FF Rx d 2 ab cd\n"
        "End measurement\n";

    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(input);
    parser.ParseData(ss);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "CAN_Data"), "AB CD");
}

// ---------------------------------------------------------------------------
// Info field is present
// ---------------------------------------------------------------------------

TEST(AscParserTest, InfoFieldPresent)
{
    AscParser parser;
    AscTestObserver obs;
    parser.RegisterObserver(&obs);

    std::istringstream ss(kSimpleAsc);
    parser.ParseData(ss);

    EXPECT_FALSE(obs.Field(0, "info").empty());
}

} // namespace test
} // namespace parser
