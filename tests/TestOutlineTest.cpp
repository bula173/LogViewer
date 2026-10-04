// analyzer::BuildTestOutline on small logs modelled on real safeAPI merged
// test logs (RBC_Test_Env results-*/…__merged_logs.txt). The lines go through
// SapiLogParser, so the events have exactly the fields of a loaded log.
#include <gtest/gtest.h>

#include "analyzers/TestOutline.hpp"
#include "sapi/SapiLogParser.hpp"
#include "EventsContainer.hpp"

#include <sstream>
#include <string>

namespace analyzer::test
{

namespace
{

class ContainerFeeder : public parser::IDataParserObserver
{
  public:
    explicit ContainerFeeder(db::EventsContainer& events) : m_events(events) {}
    void ProgressUpdated() override {}
    void NewEventFound(db::LogEvent&& ev) override { m_events.AddEvent(std::move(ev)); }
    void NewEventBatchFound(std::vector<std::pair<int, db::LogEvent::EventItems>>&& batch) override
    {
        m_events.AddEventBatch(std::move(batch));
    }

  private:
    db::EventsContainer& m_events;
};

/// Loads @p text like the application does: events + header metadata.
void Load(db::EventsContainer& events, const std::string& text)
{
    parser::SapiLogParser parser;
    ContainerFeeder       feeder(events);
    parser.RegisterObserver(&feeder);
    std::istringstream ss(text);
    parser.ParseData(ss);
    events.SetFileMetadata(parser.GetFileMetadata());
}

/// One event line at second @p secs ("SS.ffffff") of 2026-10-03 22:34.
std::string At(const std::string& secs, const std::string& rest)
{
    return "[2026-10-03T22:34:" + secs + "Z] " + rest + "\n";
}

std::string Header(const std::string& status)
{
    return "# ================================================================================\n"
           "# FULL MERGED TEST STEPS, CONTAINER & SIMULATOR LOGS\n"
           "# Test Case   : IL Grants A Movement Authority\n"
           "# Status      : " + status + "\n"
           "# ================================================================================\n";
}

const std::string kTestStart =
    "[IO] [robot] [internal] [INFO] [TEST_START] [Test execution started] [name=IL Grants A Movement Authority]";
const std::string kTestEndPass =
    "[IO] [robot] [internal] [INFO] [TEST_END] [Test finished (PASS)] [name=IL Grants A Movement Authority status=PASS]";
const std::string kTestEndFail =
    "[IO] [robot] [internal] [INFO] [TEST_END] [Test finished (FAIL)] [name=IL Grants A Movement Authority status=FAIL]";

std::string Step(const std::string& keyword, const std::string& args)
{
    return "[IO] [robot] [internal] [INFO] [STEP] [" + keyword + "] [" + args + "]";
}
std::string StepFail(const std::string& keyword, const std::string& message)
{
    return "[IO] [robot] [internal] [ERROR] [STEP_FAIL] [" + keyword + "] [" + message + "]";
}
std::string Log(const std::string& text)
{
    return "[GENERAL] [robot] [internal] [INFO] [LOG] [Log message] [" + text + "]";
}
std::string Command(const std::string& to, const std::string& cmd)
{
    return "[IO] [robot] [" + to + "] [INFO] [" + cmd + "] [Control command sent] [{\"cmd\": \"" + cmd +
           "\", \"nidEngine\": 1}]";
}
std::string Reply(const std::string& from, const std::string& status)
{
    return "[IO] [" + from + "] [robot] [INFO] [CMD_RESP] [Control command reply] [{\"status\": \"" + status + "\"}]";
}

const std::string kSutLine =
    "[GENERAL] [RBC West] [internal] [INFO] [STATUS] [[c-west | GP] self] [cycle=937 level=INFO site=WEST up=1]";

} // namespace

TEST(TestOutlineTest, LogWithoutTestStartHasNoOutline)
{
    db::EventsContainer events;
    Load(events, At("00.000000", kSutLine) + At("01.000000", kSutLine));

    const TestOutline outline = BuildTestOutline(events);
    EXPECT_FALSE(outline.HasTest());
    EXPECT_TRUE(outline.sections.empty());
    EXPECT_EQ(outline.verdict, TestVerdict::Unknown);
    EXPECT_FALSE(outline.decisiveFailureRow.has_value());
}

// A PASS log: a STEP_FAIL that was retried (Wait Until Keyword Succeeds) and
// one that was swallowed (never retried) both count as recovered.
TEST(TestOutlineTest, PassingTestMarksEveryFailureRecoveredAndSplitsSections)
{
    db::EventsContainer events;
    Load(events, Header("PASS") +
        At("00.000000", kSutLine) +                                                            // 0
        At("01.000000", kTestStart) +                                                          // 1
        At("01.000300", Step("Run Keywords", "'Assert Ready For Scenario', 'AND', 'Connect Train', '1'")) + // 2
        At("01.000600", Step("Connect Train", "'1'")) +                                        // 3
        At("01.000800", Command("Train 1", "connectTrain")) +                                  // 4
        At("01.240000", Reply("Train 1", "OK")) +                                              // 5
        At("01.250000", Log("TestStep 1: Command il-west to add a route for train 1")) +       // 6
        At("01.251000", Step("Wait Until Keyword Succeeds",
                             "'40s', '1s', 'Verify Train Received Movement Authority', '1', 2380, '1'")) + // 7
        At("01.252000", Step("Verify Train Received Movement Authority", "'1', 2380, '1'")) +  // 8
        At("01.253000", Command("Train 1", "getMessage")) +                                    // 9
        At("01.600000", Reply("Train 1", "ERR")) +                                             // 10
        At("01.601000", "[GENERAL] [robot] [internal] [FAIL] [LOG] [Log message] [3.3.ma_length for nidEngine 1 did not equal 2380]") + // 11
        At("01.602000", StepFail("Verify Train Received Movement Authority",
                                 "3.3.ma_length for nidEngine 1 did not equal 2380")) +        // 12
        At("02.602000", Step("Verify Train Received Movement Authority", "'1', 2380, '1'")) +  // 13 retry
        At("02.603000", Command("Train 1", "getMessage")) +                                    // 14
        At("02.850000", Reply("Train 1", "OK")) +                                              // 15
        At("02.851000", Log("TestExpectation 1/2: il-west accepted ADD_ROUTE")) +              // 16
        At("02.852000", Step("Reset RBC To Clean State", "")) +                                // 17
        At("02.853000", Step("Cleanup Train Sim", "")) +                                       // 18
        At("02.853100", Command("Train 1", "cleanup")) +                                       // 19
        At("03.000000", Reply("Train 1", "OK")) +                                              // 20
        At("03.100000", Step("Container Log Should Contain", "'a-west', 'IL_STATUS=DOWN'")) +  // 21
        At("03.200000", StepFail("Container Log Should Contain",
                                 "a-west's own log never showed \"IL_STATUS=DOWN\"")) +        // 22
        At("03.500000", kTestEndPass));                                                        // 23

    const TestOutline outline = BuildTestOutline(events);
    ASSERT_TRUE(outline.HasTest());
    EXPECT_EQ(outline.testName, "IL Grants A Movement Authority");
    EXPECT_EQ(outline.verdict, TestVerdict::Pass);
    EXPECT_EQ(outline.testStartRow, 1u);
    EXPECT_EQ(outline.testEndRow, 23u);
    EXPECT_FALSE(outline.decisiveFailureRow.has_value());
    EXPECT_TRUE(outline.failingStep.empty());

    ASSERT_EQ(outline.sections.size(), 3u);
    const TestStep& setup = outline.sections[0];
    EXPECT_EQ(setup.name, "Setup");
    EXPECT_EQ(setup.firstRow, 1u);
    EXPECT_EQ(setup.endRow, 6u);
    EXPECT_EQ(setup.startUs, 0);
    EXPECT_EQ(setup.durationUs, 250'000);
    EXPECT_EQ(setup.status, StepStatus::Passed);
    ASSERT_EQ(setup.children.size(), 2u);
    EXPECT_EQ(setup.children[1].name, "Connect Train");
    EXPECT_EQ(setup.children[1].args, "'1'");
    EXPECT_EQ(setup.children[1].durationUs, 249'400); // until the TestStep marker

    const TestStep& step1 = outline.sections[1];
    EXPECT_EQ(step1.name, "TestStep 1: Command il-west to add a route for train 1");
    EXPECT_EQ(step1.startUs, 250'000);
    EXPECT_EQ(step1.durationUs, 1'602'000);
    EXPECT_EQ(step1.status, StepStatus::Recovered);
    EXPECT_EQ(step1.failures, 1);
    ASSERT_EQ(step1.children.size(), 4u);
    EXPECT_EQ(step1.children[1].status, StepStatus::Recovered); // failed attempt, retried
    EXPECT_EQ(step1.children[1].failMessage, "3.3.ma_length for nidEngine 1 did not equal 2380");
    EXPECT_EQ(step1.children[2].status, StepStatus::Passed);    // the retry
    EXPECT_EQ(step1.children[3].kind, TestStep::Kind::Expectation);
    EXPECT_EQ(step1.children[3].name, "TestExpectation 1/2: il-west accepted ADD_ROUTE");

    const TestStep& teardown = outline.sections[2];
    EXPECT_EQ(teardown.name, "Teardown"); // keywords after the last TestExpectation
    EXPECT_EQ(teardown.firstRow, 17u);
    EXPECT_EQ(teardown.endRow, 23u);
    EXPECT_EQ(teardown.durationUs, 648'000);
    EXPECT_EQ(teardown.status, StepStatus::Recovered); // swallowed failure, test passed
    ASSERT_EQ(teardown.children.size(), 3u);
    EXPECT_EQ(teardown.children[2].status, StepStatus::Recovered);
}

// Wait Until Keyword Succeeds gives up: the earlier attempt is recovered (it
// was retried), the last attempt's innermost keyword failed the test.
TEST(TestOutlineTest, FinalAttemptOfARetryLoopIsTheDecisiveFailure)
{
    const std::string message = "TrainPositionInRoute.TrainPositionInRoute.route_status for nidEngine 1: expected 0, got 1";
    db::EventsContainer events;
    Load(events, Header("FAIL") +
        At("01.000000", kTestStart) +                                                          // 0
        At("01.100000", Log("TestStep 1: Train leaves the route")) +                           // 1
        At("01.200000", Step("Wait Until Keyword Succeeds",
                             "'40s', '1s', 'Verify IL No Train In Route Status', '1'")) +      // 2
        At("01.200500", Step("Verify IL No Train In Route Status", "'1'")) +                   // 3
        At("01.201000", Step("Verify IL Packet", "'1', 'TrainPositionInRoute', 'route_status', '0'")) + // 4
        At("01.202000", StepFail("Verify IL Packet", message)) +                               // 5
        At("01.202100", StepFail("Verify IL No Train In Route Status", message)) +             // 6
        At("02.202000", Step("Verify IL No Train In Route Status", "'1'")) +                   // 7
        At("02.202500", Step("Verify IL Packet", "'1', 'TrainPositionInRoute', 'route_status', '0'")) + // 8
        At("02.203000", StepFail("Verify IL Packet", message)) +                               // 9
        At("02.203100", StepFail("Verify IL No Train In Route Status", message)) +             // 10
        At("02.203500", StepFail("Wait Until Keyword Succeeds",
                                 "Keyword 'Verify IL No Train In Route Status' failed after retrying for 40 seconds.")) + // 11
        At("02.204000", Step("Reset RBC To Clean State", "")) +                                // 12
        At("02.500000", kTestEndFail));                                                        // 13

    const TestOutline outline = BuildTestOutline(events);
    EXPECT_EQ(outline.verdict, TestVerdict::Fail);
    EXPECT_EQ(outline.decisiveFailureRow, 9u);
    EXPECT_EQ(outline.failingStep, "Verify IL Packet");
    EXPECT_EQ(outline.failureMessage, message);

    ASSERT_EQ(outline.sections.size(), 1u); // empty Setup is dropped, no expectation → no Teardown
    const TestStep& step = outline.sections[0];
    EXPECT_EQ(step.status, StepStatus::Failed);
    EXPECT_EQ(step.failures, 5);
    ASSERT_EQ(step.children.size(), 6u);
    EXPECT_EQ(step.children[0].status, StepStatus::Failed);    // Wait Until Keyword Succeeds
    EXPECT_EQ(step.children[1].status, StepStatus::Recovered); // attempt 1
    EXPECT_EQ(step.children[2].status, StepStatus::Recovered);
    EXPECT_EQ(step.children[3].status, StepStatus::Failed);    // attempt 2
    EXPECT_EQ(step.children[4].status, StepStatus::Failed);
    EXPECT_EQ(step.children[5].status, StepStatus::Passed);    // teardown keyword, sends no commands
}

// results-20261003-085236-44867: the suite setup failed, so the test log holds
// nothing but TEST_START and TEST_END (FAIL), 2 ms apart.
TEST(TestOutlineTest, FailedTestWithoutStepFailPointsAtTestEnd)
{
    db::EventsContainer events;
    Load(events, Header("FAIL") +
        At("59.486162", kSutLine) +
        At("59.486162", kTestStart) +
        At("59.488179", kTestEndFail));

    const TestOutline outline = BuildTestOutline(events);
    ASSERT_TRUE(outline.HasTest());
    EXPECT_EQ(outline.verdict, TestVerdict::Fail);
    EXPECT_EQ(outline.decisiveFailureRow, 2u);
    EXPECT_TRUE(outline.failingStep.empty());
    ASSERT_EQ(outline.sections.size(), 1u);
    EXPECT_EQ(outline.sections[0].name, "Test");
    EXPECT_TRUE(outline.sections[0].children.empty());
    EXPECT_EQ(outline.sections[0].durationUs, 2'017);
}

// The failing handover run: after "Wait For Train Message" fails, Robot logs
// the rest of the body without running it (no commands), then the teardown runs.
TEST(TestOutlineTest, StepsAfterTheDecisiveFailureWithoutCommandsAreProbablyNotRun)
{
    db::EventsContainer events;
    Load(events, Header("FAIL") +
        At("35.000000", kTestStart) +                                                          // 0
        At("35.000200", Log("TestStep 1: Connect train 1")) +                                  // 1
        At("35.000400", Step("Set Train Variable", "'1', 'nid_lrbg', '1001'")) +               // 2
        At("35.000600", Command("Train 1", "setTrainVariable")) +                              // 3
        At("35.400000", Reply("Train 1", "OK")) +                                              // 4
        At("35.401000", Step("Send Train Message", "'1', '155'")) +                            // 5
        At("35.401200", Command("Train 1", "sendTrainMessage")) +                              // 6
        At("35.800000", Reply("Train 1", "OK")) +                                              // 7
        At("39.000000", Log("TestStep 2: Establish session in West RBC")) +                    // 8
        At("39.100000", Step("Wait For Train Message", "'1', '24', '20s', '0.2s'")) +          // 9
        At("39.100200", Command("Train 1", "getMessage")) +                                    // 10
        At("59.632567", Reply("Train 1", "ERR")) +                                             // 11
        At("59.633873", "[GENERAL] [robot] [internal] [FAIL] [LOG] [Log message] [24 for nidEngine 1 (train) not received within 20s]") + // 12
        At("59.639434", StepFail("Wait For Train Message",
                                 "24 for nidEngine 1 (train) not received within 20s: no 24 received yet for nidEngine 1")) + // 13
        At("59.639800", Step("Send Train Message", "'1', '146'")) +                            // 14
        At("59.640006", Log("TestExpectation 1: Session established in West RBC")) +           // 15
        At("59.640322", Log("TestStep 3: Acknowledge session")) +                              // 16
        At("59.640552", Step("Set Train Variable", "'1', 'ma_seq', '0'")) +                    // 17
        At("59.640736", Step("Send Train Message", "'1', '146'")) +                            // 18
        At("59.641923", Log("TestExpectation 2: Handover boundary position transmitted")) +    // 19
        At("59.642897", Step("Reset RBC To Clean State", "")) +                                // 20
        At("59.644069", Step("Cleanup Train Sim", "")) +                                       // 21
        At("59.644176", Command("Train 1", "cleanup")) +                                       // 22
        At("59.958854", Reply("Train 1", "OK")) +                                              // 23
        At("59.990000", kTestEndFail));                                                        // 24

    const TestOutline outline = BuildTestOutline(events);
    EXPECT_EQ(outline.decisiveFailureRow, 13u);
    EXPECT_EQ(outline.failingStep, "Wait For Train Message");

    ASSERT_EQ(outline.sections.size(), 4u);
    EXPECT_EQ(outline.sections[0].name, "TestStep 1: Connect train 1");
    EXPECT_EQ(outline.sections[0].status, StepStatus::Passed);

    const TestStep& failing = outline.sections[1];
    EXPECT_EQ(failing.status, StepStatus::Failed);
    ASSERT_EQ(failing.children.size(), 3u);
    EXPECT_EQ(failing.children[0].status, StepStatus::Failed);
    EXPECT_EQ(failing.children[0].durationUs, 20'539'800); // the 20 s timeout is visible
    EXPECT_EQ(failing.children[1].status, StepStatus::NotRun); // sends commands elsewhere, none here
    EXPECT_EQ(failing.children[2].status, StepStatus::NotRun); // expectation never verified

    const TestStep& skipped = outline.sections[2];
    EXPECT_EQ(skipped.name, "TestStep 3: Acknowledge session");
    EXPECT_EQ(skipped.status, StepStatus::NotRun);
    ASSERT_EQ(skipped.children.size(), 3u);
    for (const auto& child : skipped.children)
        EXPECT_EQ(child.status, StepStatus::NotRun);

    const TestStep& teardown = outline.sections[3];
    EXPECT_EQ(teardown.name, "Teardown");
    EXPECT_EQ(teardown.status, StepStatus::Passed);
    ASSERT_EQ(teardown.children.size(), 2u);
    EXPECT_EQ(teardown.children[0].status, StepStatus::Passed); // never sends commands itself
    EXPECT_EQ(teardown.children[1].status, StepStatus::Passed);
}

// results-20261003-085948-55643/After_Cold_Switch…: the setup failed, then the
// teardown failed too. The setup failure is what failed the test.
TEST(TestOutlineTest, FirstUnrecoveredFailureIsDecisiveNotALaterTeardownFailure)
{
    const std::string stack = "stack not in the state this scenario assumes after 15s";
    db::EventsContainer events;
    Load(events, Header("FAIL") +
        At("00.313855", kTestStart) +                                                          // 0
        At("00.314000", Step("Run Keywords", "'Assert Ready For Scenario', 'AND', 'Connect Train', '1'")) + // 1
        At("00.314200", Step("Assert Ready For Scenario", "")) +                               // 2
        At("16.753914", StepFail("Assert Ready For Scenario", stack)) +                        // 3
        At("16.754562", StepFail("Run Keywords", stack)) +                                     // 4
        At("16.800000", Step("Ensure West Is Online", "")) +                                   // 5
        At("23.591187", StepFail("Ensure West Is Online", "a-west is not ONLINE yet")) +       // 6
        At("23.592968", kTestEndFail));                                                        // 7

    const TestOutline outline = BuildTestOutline(events);
    EXPECT_EQ(outline.decisiveFailureRow, 3u);
    EXPECT_EQ(outline.failingStep, "Assert Ready For Scenario");
    EXPECT_EQ(outline.failureMessage, stack);
    ASSERT_EQ(outline.sections.size(), 1u);
    ASSERT_EQ(outline.sections[0].children.size(), 3u);
    EXPECT_EQ(outline.sections[0].children[0].status, StepStatus::Failed);
    EXPECT_EQ(outline.sections[0].children[1].status, StepStatus::Failed);
    EXPECT_EQ(outline.sections[0].children[2].status, StepStatus::Failed); // also failed, not decisive
}

// Some STEP_FAILs name a keyword that never started (e.g. the test's own name).
TEST(TestOutlineTest, FailureWithoutMatchingKeywordStillFailsItsSection)
{
    db::EventsContainer events;
    Load(events, Header("FAIL") +
        At("01.000000", kTestStart) +
        At("01.100000", Log("TestStep 1: Silence the counterpart")) +
        At("55.584892", StepFail("Takeover Confirmation With Wrong Key Is Rejected",
                                 "a-east has not reported the silent counterpart yet")) +
        At("56.892855", kTestEndFail));

    const TestOutline outline = BuildTestOutline(events);
    EXPECT_EQ(outline.decisiveFailureRow, 2u);
    EXPECT_EQ(outline.failingStep, "Takeover Confirmation With Wrong Key Is Rejected");
    ASSERT_EQ(outline.sections.size(), 1u);
    EXPECT_EQ(outline.sections[0].status, StepStatus::Failed);
    EXPECT_EQ(outline.sections[0].failures, 1);
    EXPECT_EQ(outline.sections[0].failMessage, "a-east has not reported the silent counterpart yet");
}

TEST(TestOutlineTest, WithoutHeaderVerdictAndNameComeFromTestEvents)
{
    db::EventsContainer events;
    Load(events,
        At("01.000000", kTestStart) +
        At("01.100000", Step("Wait For CTC Message", "'1', 'CTC_MA_GRANTED', '40s', '1s'")) +
        At("41.100000", StepFail("Wait For CTC Message", "CTC_MA_GRANTED for nidEngine 1 (ctc) not received within 40s")) +
        At("43.000000", kTestEndFail));

    const TestOutline outline = BuildTestOutline(events);
    EXPECT_EQ(outline.testName, "IL Grants A Movement Authority");
    EXPECT_EQ(outline.verdict, TestVerdict::Fail);
    EXPECT_EQ(outline.decisiveFailureRow, 2u);
}

TEST(TestOutlineTest, LogEndingInsideTheTestHasNoVerdict)
{
    db::EventsContainer events;
    Load(events,
        At("01.000000", kTestStart) +
        At("01.100000", Step("Wait For CTC Message", "'1', 'CTC_MA_GRANTED', '40s', '1s'")) +
        At("41.100000", StepFail("Wait For CTC Message", "not received within 40s")));

    const TestOutline outline = BuildTestOutline(events);
    EXPECT_EQ(outline.verdict, TestVerdict::Unknown);
    EXPECT_FALSE(outline.testEndRow.has_value());
    EXPECT_FALSE(outline.decisiveFailureRow.has_value());
    ASSERT_EQ(outline.sections.size(), 1u);
    EXPECT_EQ(outline.sections[0].endRow, 3u);
    EXPECT_EQ(outline.sections[0].children[0].status, StepStatus::Failed);
}

} // namespace analyzer::test
