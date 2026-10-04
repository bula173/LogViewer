// analyzer::TestRunIndex on small synthetic results folders modelled on the
// real safeAPI runs (RBC_Test_Env results-*/): one *__merged_logs.txt per test
// plus Robot Framework's xunit.xml / output.xml.
#include <gtest/gtest.h>

#include "analyzers/TestRunIndex.hpp"

#include <QTemporaryDir>

#include <filesystem>
#include <fstream>
#include <string>

namespace analyzer::test
{

namespace
{

namespace fs = std::filesystem;

void WriteFile(const fs::path& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary);
    out << text;
}

std::string Header(const std::string& name, const std::string& status,
                   const std::string& timestamp, int entries)
{
    return "# ================================================================================\n"
           "# FULL MERGED TEST STEPS, CONTAINER & SIMULATOR LOGS\n"
           "# Test Case   : " + name + "\n"
           "# Status      : " + status + "\n"
           "# Timestamp   : " + timestamp + "\n"
           "# Sources     : robot, a-west, il\n"
           "# Entries     : " + std::to_string(entries) + "\n"
           "# Line Format : [timestamp] [category] [source] [destination] [log level] [event type] [info] [payload]\n"
           "# ================================================================================\n";
}

/// One event line at second @p secs ("SS.ffffff") of 2026-10-03 22:34.
std::string At(const std::string& secs, const std::string& rest)
{
    return "[2026-10-03T22:34:" + secs + "Z] " + rest + "\n";
}

std::string Housekeeping(const std::string& secs)
{
    return At(secs, "[GENERAL] [RBC West] [internal] [INFO] [LOG] [[a-west | GP] Log message] "
                    "[cycle 29: negotiation ok]");
}

std::string TestStart(const std::string& secs, const std::string& name)
{
    return At(secs, "[IO] [robot] [internal] [INFO] [TEST_START] [Test execution started] [name=" + name + "]");
}

std::string TestEnd(const std::string& secs, const std::string& name, const std::string& status)
{
    return At(secs, "[IO] [robot] [internal] [INFO] [TEST_END] [Test finished (" + status + ")] [name=" +
                    name + " status=" + status + "]");
}

std::string Step(const std::string& secs, const std::string& keyword, const std::string& args)
{
    return At(secs, "[IO] [robot] [internal] [INFO] [STEP] [" + keyword + "] [" + args + "]");
}

std::string StepFail(const std::string& secs, const std::string& keyword, const std::string& message)
{
    return At(secs, "[IO] [robot] [internal] [ERROR] [STEP_FAIL] [" + keyword + "] [" + message + "]");
}

/// PASS, ran second. Its recovered STEP_FAIL must not produce a message.
void WritePassLog(const fs::path& dir)
{
    const std::string name = "IL Grants A Movement Authority";
    WriteFile(dir / "IL_Grants_A_Movement_Authority__merged_logs.txt",
              Header(name, "PASS", "2026-10-03T22:34:20.000000+00:00", 6) +
              Housekeeping("10.000000") +
              TestStart("11.000000", name) +
              Step("12.000000", "Wait For CTC Message", "'1'") +
              StepFail("13.000000", "Wait For CTC Message", "not yet") +
              Step("14.000000", "Wait For CTC Message", "'1'") +
              TestEnd("15.500000", name, "PASS"));
}

/// FAIL with a decisive STEP_FAIL followed by a teardown failure; ran third.
void WriteStepFailLog(const fs::path& dir, const std::string& file, const std::string& name,
                      const std::string& timestamp, const std::string& message)
{
    WriteFile(dir / file,
              Header(name, "FAIL", timestamp, 7) +
              TestStart("30.000000", name) +
              Step("31.000000", "Wait For Train Message", "'1', '24', '20s'") +
              StepFail("51.000000", "Wait For Train Message", message) +
              Step("52.000000", "Ensure West Is Online", "") +
              StepFail("53.000000", "Ensure West Is Online", "teardown broke too") +
              TestEnd("54.000000", name, "FAIL") +
              Housekeeping("55.250000"));
}

/// FAIL in the suite setup: only TEST_START / TEST_END; ran first.
void WriteSetupFailLog(const fs::path& dir)
{
    const std::string name = "No Container Is Crash-Looping";
    WriteFile(dir / "No_Container_Is_Crash-Looping__merged_logs.txt",
              Header(name, "FAIL", "2026-10-03T22:34:05.000000+00:00", 2) +
              TestStart("01.000000", name) +
              TestEnd("01.002000", name, "FAIL"));
}

const char* const kXunit = R"(<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="Robot" tests="3" errors="0" failures="2" skipped="0" time="3.0">
<testsuite name="Containers" tests="1" failures="1">
<testcase classname="Robot.Containers.No Container Is Crash Looping" name="No Container Is Crash-Looping" time="0.000">
<failure message="Parent suite setup failed:&#10;a-west has not seen the probe train connect yet" type="AssertionError"/>
</testcase>
</testsuite>
<testcase classname="Robot.X" name="IL Grants A Movement Authority" time="4.5">
</testcase>
</testsuite>
)";

const char* const kOutputXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<robot generator="Robot 7.5" schemaversion="5">
<suite id="s1" name="Containers">
<status status="FAIL" start="2026-10-03T22:34:00.000000" elapsed="9.0">suite setup failed</status>
<test id="s1-t1" name="No Container Is Crash-Looping" line="17">
<kw name="Ensure Stack Is Up" owner="Support">
<status status="FAIL" start="2026-10-03T22:34:01.000000" elapsed="0.001">keyword level message</status>
</kw>
<status status="FAIL" start="2026-10-03T22:34:01.000000" elapsed="0.002">Parent suite setup failed:
probe train never connected</status>
</test>
</suite>
</robot>
)";

class TestRunIndexTest : public ::testing::Test
{
  protected:
    void SetUp() override { ASSERT_TRUE(m_dir.isValid()); }
    fs::path Dir() const { return fs::path(m_dir.path().toStdString()); }

    /// The three-test run: setup failure, PASS, STEP_FAIL failure.
    void WriteRun()
    {
        WriteSetupFailLog(Dir());
        WritePassLog(Dir());
        WriteStepFailLog(Dir(), "ERTMS_RBC_Handover__merged_logs.txt", "ERTMS RBC Handover",
                         "2026-10-03T22:34:59.000000+00:00", "24 not received within 20s");
        WriteFile(Dir() / "xunit.xml", kXunit);
        WriteFile(Dir() / "log.html", "<html></html>");
        WriteFile(Dir() / "notes.txt", "not a test log");
    }

    QTemporaryDir m_dir;
};

} // namespace

TEST_F(TestRunIndexTest, SummaryReadsTheHeaderAndTheTimeSpan)
{
    WritePassLog(Dir());
    const TestRunEntry e = ReadTestLogSummary(Dir() / "IL_Grants_A_Movement_Authority__merged_logs.txt");
    EXPECT_EQ(e.testName, "IL Grants A Movement Authority");
    EXPECT_EQ(e.verdict, TestVerdict::Pass);
    EXPECT_EQ(e.timestamp, "2026-10-03T22:34:20.000000+00:00");
    EXPECT_EQ(e.entries, 6);
    EXPECT_EQ(e.durationUs, 5'500'000); // 22:34:10.0 … 22:34:15.5
    EXPECT_TRUE(e.failureMessage.empty());
}

TEST_F(TestRunIndexTest, SummaryWithoutHeaderFallsBackToTheFileAndTheFirstEvent)
{
    WriteFile(Dir() / "Some_Test__merged_logs.txt", Housekeeping("10.000000") + Housekeeping("12.000000"));
    const TestRunEntry e = ReadTestLogSummary(Dir() / "Some_Test__merged_logs.txt");
    EXPECT_EQ(e.testName, "Some Test");
    EXPECT_EQ(e.verdict, TestVerdict::Unknown);
    EXPECT_EQ(e.timestamp, "2026-10-03T22:34:10.000000Z");
    EXPECT_EQ(e.entries, -1);
    EXPECT_EQ(e.durationUs, 2'000'000);
}

TEST_F(TestRunIndexTest, DecisiveFailureIsTheFirstUnrecoveredStepFail)
{
    WriteStepFailLog(Dir(), "T__merged_logs.txt", "T", "2026-10-03T22:34:59+00:00",
                     "24 not received\twithin 20s");
    EXPECT_EQ(FindDecisiveFailureMessage(Dir() / "T__merged_logs.txt"), "24 not received within 20s");
}

TEST_F(TestRunIndexTest, SetupFailureLogHasNoDecisiveStepFail)
{
    WriteSetupFailLog(Dir());
    EXPECT_EQ(FindDecisiveFailureMessage(Dir() / "No_Container_Is_Crash-Looping__merged_logs.txt"), "");
}

TEST_F(TestRunIndexTest, ScanListsOnlyMergedLogsInRunOrderWithFailureMessages)
{
    WriteRun();
    int calls = 0;
    const auto entries = ScanResultsFolder(Dir(), [&](std::size_t done, std::size_t total) {
        ++calls;
        EXPECT_EQ(total, 3u);
        EXPECT_LE(done, total);
        return true;
    });
    EXPECT_EQ(calls, 3);

    ASSERT_EQ(entries.size(), 3u);
    // Run order follows the header Timestamp, not the file name.
    EXPECT_EQ(entries[0].testName, "No Container Is Crash-Looping");
    EXPECT_EQ(entries[1].testName, "IL Grants A Movement Authority");
    EXPECT_EQ(entries[2].testName, "ERTMS RBC Handover");

    // Setup failure: no STEP_FAIL in the log, the message comes from xunit.xml.
    EXPECT_EQ(entries[0].verdict, TestVerdict::Fail);
    EXPECT_EQ(entries[0].failureMessage, "Parent suite setup failed: a-west has not seen the probe train connect yet");
    EXPECT_EQ(entries[0].failureSource, FailureSource::RobotXml);

    EXPECT_EQ(entries[1].verdict, TestVerdict::Pass);
    EXPECT_EQ(entries[1].failureMessage, "");
    EXPECT_EQ(entries[1].failureSource, FailureSource::None);

    EXPECT_EQ(entries[2].failureMessage, "24 not received within 20s");
    EXPECT_EQ(entries[2].failureSource, FailureSource::StepFail);
    EXPECT_EQ(entries[2].file.filename(), "ERTMS_RBC_Handover__merged_logs.txt");
}

TEST_F(TestRunIndexTest, WithoutXunitTheTestStatusOfOutputXmlIsUsed)
{
    WriteSetupFailLog(Dir());
    WriteFile(Dir() / "output.xml", kOutputXml);
    const auto entries = ScanResultsFolder(Dir());
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].failureMessage, "Parent suite setup failed: probe train never connected");
    EXPECT_EQ(entries[0].failureSource, FailureSource::RobotXml);
}

TEST_F(TestRunIndexTest, BrokenOrMissingRobotFilesGiveNoMessage)
{
    WriteSetupFailLog(Dir());
    WriteFile(Dir() / "output.xml", ""); // a real run left an empty output.xml
    const auto entries = ScanResultsFolder(Dir());
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].failureMessage, "");
    EXPECT_EQ(entries[0].failureSource, FailureSource::None);
    EXPECT_TRUE(ScanResultsFolder(Dir() / "missing").empty());
}

TEST_F(TestRunIndexTest, CancelledScanReturnsNothing)
{
    WriteRun();
    EXPECT_TRUE(ScanResultsFolder(Dir(), [](std::size_t, std::size_t) { return false; }).empty());
}

TEST(TestRunIndexHelpers, NamesMatchAcrossHeaderXunitAndFileName)
{
    EXPECT_EQ(TestNameKey("No Container Is Crash-Looping"), TestNameKey("No_Container_Is_Crash-Looping"));
    EXPECT_EQ(TestNameKey("CTC Oversees Both Sites"), "ctcoverseesbothsites");
}

TEST(TestRunIndexHelpers, NormalisationCollapsesWhitespaceAndDigits)
{
    EXPECT_EQ(CollapseWhitespace("  a\n b\t\tc  "), "a b c");
    EXPECT_EQ(NormalizeFailureMessage("24 not received\nwithin 20s (cycle 1234)"),
              "N not received within Ns (cycle N)");
    EXPECT_EQ(NormalizeFailureMessage("nidEngine 1"), NormalizeFailureMessage("nidEngine 2"));
}

TEST(TestRunIndexHelpers, GroupFailuresByNormalisedMessageLargestFirst)
{
    const auto fail = [](const std::string& message) {
        TestRunEntry e;
        e.verdict        = TestVerdict::Fail;
        e.failureMessage = message;
        return e;
    };
    TestRunEntry pass;
    pass.verdict = TestVerdict::Pass;

    const std::vector<TestRunEntry> entries{
        fail("setup failed"),               // 0
        fail("24 not received within 20s"), // 1
        pass,                               // 2
        fail("3 not received within 40s"),  // 3
        fail(""),                           // 4: no message, not grouped
    };
    const auto groups = GroupFailures(entries);
    ASSERT_EQ(groups.size(), 2u);
    EXPECT_EQ(groups[0].message, "24 not received within 20s");
    EXPECT_EQ(groups[0].members, (std::vector<std::size_t>{1, 3}));
    EXPECT_EQ(groups[1].message, "setup failed");
    EXPECT_EQ(groups[1].members, (std::vector<std::size_t>{0}));
}

} // namespace analyzer::test
