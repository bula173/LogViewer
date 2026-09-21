// Regression tests for the v1.13.1 audit fixes in the safeAPI parser sniffing / BOM handling.
#include <gtest/gtest.h>
#include "sapi/SapiLogParser.hpp"
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
class RobustnessCollector : public IDataParserObserver
{
  public:
    size_t count {0};
    void ProgressUpdated() override {}
    void NewEventFound(db::LogEvent&&) override { ++count; }
    void NewEventBatchFound(std::vector<std::pair<int, db::LogEvent::EventItems>>&& batch) override
    { count += batch.size(); }
};

std::filesystem::path WriteTemp(const std::string& name, const std::string& content)
{
    const auto path = std::filesystem::temp_directory_path() / ("lv_sapi_robust_" + name);
    std::ofstream(path, std::ios::binary) << content;
    return path;
}

constexpr const char* kEvent =
    "[2026-09-19T06:46:46.456779Z] [GENERAL] [a] [b] [INFO] [LOG] [hello] [payload]\n";
} // namespace

TEST(SapiLogParserRobustnessTest, HeaderWithUtf8BomIsDetected)
{
    const auto path = WriteTemp("bom.txt", std::string("\xEF\xBB\xBF# FULL MERGED TEST STEPS\n") + kEvent);
    EXPECT_TRUE(SapiLogParser::LooksLikeSapiLog(path));
    std::filesystem::remove(path);
}

TEST(SapiLogParserRobustnessTest, FirstEventAfterBomIsNotLost)
{
    RobustnessCollector col;
    SapiLogParser parser;
    parser.RegisterObserver(&col);
    std::istringstream in(std::string("\xEF\xBB\xBF") + kEvent + kEvent);
    parser.ParseData(in);
    EXPECT_EQ(col.count, 2u);
}

TEST(SapiLogParserRobustnessTest, HugeLineWithoutNewlineIsNotSniffedAsSapi)
{
    // Larger than the sniff window: must return promptly and false, not slurp the file.
    const auto path = WriteTemp("blob.txt", std::string(2 * 1024 * 1024, 'x'));
    EXPECT_FALSE(SapiLogParser::LooksLikeSapiLog(path));
    std::filesystem::remove(path);
}

TEST(SapiLogParserRobustnessTest, MarkerOnTheFifthLineIsStillDetected)
{
    const auto path = WriteTemp("late.txt", "a\nb\nc\nd\n# FULL MERGED TEST STEPS\n");
    EXPECT_TRUE(SapiLogParser::LooksLikeSapiLog(path));
    std::filesystem::remove(path);
}

TEST(SapiLogParserRobustnessTest, MarkerBeyondTheProbeLinesIsIgnored)
{
    const auto path = WriteTemp("toolate.txt", "a\nb\nc\nd\ne\n# FULL MERGED TEST STEPS\n");
    EXPECT_FALSE(SapiLogParser::LooksLikeSapiLog(path));
    std::filesystem::remove(path);
}

TEST(SapiLogParserRobustnessTest, MissingFileIsNotSapi)
{
    EXPECT_FALSE(SapiLogParser::LooksLikeSapiLog("/nonexistent/definitely/not/here.txt"));
}

} // namespace parser::test
