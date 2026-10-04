#pragma once

#include "TestOutline.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace analyzer
{

/// File-name suffix of a safeAPI merged test log inside a results folder.
inline constexpr std::string_view kMergedLogSuffix = "__merged_logs.txt";

/// Where TestRunEntry::failureMessage came from.
enum class FailureSource
{
    None,      ///< no message found (or the test did not fail)
    StepFail,  ///< the decisive STEP_FAIL of the log (see BuildTestOutline)
    RobotXml   ///< `xunit.xml` `<failure message>` or `output.xml` test status text
};

/// One test of a results folder, read without loading the whole log.
struct TestRunEntry
{
    std::filesystem::path file;
    std::string  testName;      ///< header `Test Case`, else the file name without the suffix
    TestVerdict  verdict {TestVerdict::Unknown}; ///< header `Status`
    std::string  timestamp;     ///< header `Timestamp`, else the first event's timestamp
    std::int64_t durationUs {-1}; ///< last − first event timestamp; -1 = unknown
    std::int64_t entries {-1};    ///< header `Entries`; -1 = absent
    std::string  failureMessage;  ///< FAIL only, on one line (whitespace collapsed)
    FailureSource failureSource {FailureSource::None};
};

/// Header and time span of one merged log: reads the `#` header block, the
/// first event line and the last 64 KiB of the file. Never parses the events.
[[nodiscard]] TestRunEntry ReadTestLogSummary(const std::filesystem::path& file);

/// Message of the decisive STEP_FAIL of a failing merged log (empty when the
/// log records none, e.g. a suite-setup failure). Reads every line but parses
/// only the robot marker lines (TEST_START / TEST_END / STEP / STEP_FAIL) and
/// hands them to BuildTestOutline, so the choice matches the Test Steps tab.
[[nodiscard]] std::string FindDecisiveFailureMessage(const std::filesystem::path& file);

/// Failure messages Robot Framework wrote next to the logs: test name (see
/// TestNameKey) → message. `xunit.xml` (`<testcase name>` / `<failure message>`
/// or `<error message>`) is used when it exists, else `output.xml` (the text
/// of a `<test>`'s own FAIL `<status>`). Missing or broken files give an empty map.
[[nodiscard]] std::unordered_map<std::string, std::string>
ReadRobotFailures(const std::filesystem::path& folder);

/// Matching key for test names: lower-case letters and digits only, so the
/// header name "No Container Is Crash-Looping", the xunit name and the file
/// name "No_Container_Is_Crash-Looping" all match.
[[nodiscard]] std::string TestNameKey(std::string_view name);

/// Collapses whitespace runs (including newlines) to one space and trims.
[[nodiscard]] std::string CollapseWhitespace(std::string_view text);

/// Grouping signature of a failure message: whitespace collapsed and every
/// run of digits replaced by `N`, so "not received within 20s" and
/// "not received within 40s" fall into one group.
[[nodiscard]] std::string NormalizeFailureMessage(std::string_view message);

/// Called after each scanned log with (done, total); return false to cancel.
using ScanProgress = std::function<bool(std::size_t done, std::size_t total)>;

/// Scans every `*__merged_logs.txt` of @p folder (not recursive):
/// ReadTestLogSummary for each, FindDecisiveFailureMessage for FAIL logs only,
/// ReadRobotFailures for FAIL logs still without a message.
///
/// The result is in run order: by `timestamp` (ISO-8601 strings of one run
/// share a time zone, so they compare as text), then by file name. The test
/// that ran before entry i is therefore entry i − 1.
///
/// Returns an empty vector when @p progress cancels.
[[nodiscard]] std::vector<TestRunEntry> ScanResultsFolder(const std::filesystem::path& folder,
                                                          const ScanProgress& progress = {});

/// Failing tests sharing one normalised message.
struct FailureGroup
{
    std::string message;              ///< message of the first member
    std::vector<std::size_t> members; ///< indices into the entries, in entry order
};

/// Groups the FAIL entries that have a message by NormalizeFailureMessage;
/// largest group first, ties in order of first appearance.
[[nodiscard]] std::vector<FailureGroup> GroupFailures(const std::vector<TestRunEntry>& entries);

} // namespace analyzer
