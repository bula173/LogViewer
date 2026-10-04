#pragma once

#include "EventsContainer.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace analyzer
{

/// File-metadata keys of a safeAPI merged-log header (see SapiLogParser).
inline constexpr std::string_view kMetaTestCase = "Test Case";
inline constexpr std::string_view kMetaStatus   = "Status";

/// The event vocabulary of a test-harness log. Defaults match the Robot
/// Framework listener output of safeAPI merged logs:
///
/// @code
/// [ts] [IO] [robot] [internal] [INFO] [TEST_START] [Test execution started] [name=…]
/// [ts] [IO] [robot] [internal] [INFO] [STEP] [Wait For CTC Message] ['1', 'CTC_MA_GRANTED', '40s', '1s']
/// [ts] [IO] [robot] [CTC] [INFO] [getMessage] [Control command sent] [{"cmd": "getMessage", …}]
/// [ts] [GENERAL] [robot] [internal] [INFO] [LOG] [Log message] [TestStep 3: Wait for CTC's own indication]
/// [ts] [IO] [robot] [internal] [ERROR] [STEP_FAIL] [Wait For CTC Message] [CTC_MA_GRANTED … not received within 40s]
/// [ts] [IO] [robot] [internal] [INFO] [TEST_END] [Test finished (FAIL)] [name=… status=FAIL]
/// @endcode
struct TestMarkerRules
{
    std::string timeField  = "timestamp";
    std::string typeField  = "event_type";
    std::string nameField  = "info";    ///< keyword name (STEP, STEP_FAIL), TEST_END verdict text
    std::string textField  = "payload"; ///< keyword arguments, failure message, marker text, test name

    std::string testStart   = "TEST_START";
    std::string testEnd     = "TEST_END";
    std::string keyword     = "STEP";      ///< a keyword starts
    std::string keywordFail = "STEP_FAIL"; ///< a keyword failed
    std::string log         = "LOG";       ///< carries the author's step markers

    std::string stepMarker        = "TestStep";        ///< LOG text prefix opening a section
    std::string expectationMarker = "TestExpectation"; ///< LOG text prefix of a checked expectation
    std::string commandName       = "Control command sent"; ///< nameField of a command to a simulator
};

enum class StepStatus
{
    Passed,    ///< ran, no failure recorded
    Recovered, ///< failed at least once, but the failure did not fail the test (retried, or the test passed)
    Failed,    ///< failed and was never retried in a failing test
    NotRun     ///< heuristic: logged after the decisive failure without doing anything
};

enum class TestVerdict
{
    Unknown,
    Pass,
    Fail
};

struct TestStep
{
    enum class Kind
    {
        Section,     ///< Setup, one `TestStep N: …` marker, or Teardown
        Keyword,     ///< one STEP event
        Expectation  ///< one `TestExpectation …` marker
    };

    Kind        kind {Kind::Keyword};
    std::string name;   ///< section title, keyword name or marker text
    std::string args;   ///< keyword arguments (keywords only)
    std::size_t firstRow {0}; ///< EventsContainer index of the step's first event
    std::size_t endRow   {0}; ///< one past the step's last event
    std::int64_t startUs    {-1}; ///< start relative to TEST_START in µs; -1 = no timestamp
    std::int64_t durationUs {-1}; ///< until the next step starts, in µs; -1 = unknown
    StepStatus  status {StepStatus::Passed};
    int         failures {0};  ///< STEP_FAIL events of this step (sections: of their children)
    std::string failMessage;   ///< message of the step's last failure
    std::vector<TestStep> children; ///< keywords and expectations of a section
};

struct TestOutline
{
    std::string testName;
    TestVerdict verdict {TestVerdict::Unknown};
    std::optional<std::size_t> testStartRow;
    std::optional<std::size_t> testEndRow;
    /// FAIL only: the STEP_FAIL that failed the test, or TEST_END when the log
    /// records no failing keyword (e.g. the suite setup failed elsewhere).
    std::optional<std::size_t> decisiveFailureRow;
    std::string failingStep;    ///< keyword name of the decisive failure (empty: none recorded)
    std::string failureMessage; ///< its message
    std::vector<TestStep> sections;

    /// True when the events contain a test (TEST_START), i.e. there is an outline to show.
    [[nodiscard]] bool HasTest() const { return testStartRow.has_value(); }
};

/// Builds the step outline of the first test in @p events.
///
/// - Sections: "Setup" up to the first `TestStep` marker, one section per
///   marker, and "Teardown" for the keywords after the last `TestExpectation`
///   marker (only when that is the last marker). Without `TestStep` markers
///   everything up to the teardown is one section, "Test".
/// - A STEP_FAIL belongs to the latest not-yet-failed STEP of the same keyword.
///   It is *recovered* when the same keyword with the same arguments starts
///   again later (a retry, e.g. under `Wait Until Keyword Succeeds`); in a
///   passing test every failure counts as recovered.
/// - Verdict: the header `Status` (file metadata), else `(PASS)` / `(FAIL)` in
///   the TEST_END text.
/// - Decisive failure (FAIL only): the first STEP_FAIL that is not recovered —
///   the innermost keyword of the failing chain, never a teardown failure that
///   followed it. Without one, TEST_END.
/// - Not run (heuristic): after the decisive failure, a section without any
///   command, an expectation, or a keyword that neither failed nor sent a
///   command although the same keyword sent one elsewhere in the log.
[[nodiscard]] TestOutline BuildTestOutline(db::EventsContainer& events,
                                           const TestMarkerRules& rules = {});

} // namespace analyzer
