#include "TestOutline.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <iterator>
#include <unordered_map>
#include <unordered_set>

namespace analyzer
{

std::optional<std::int64_t> ParseIsoMicros(std::string_view ts)
{
    if (ts.size() < 19 || ts[4] != '-' || ts[7] != '-' || ts[13] != ':' || ts[16] != ':')
        return std::nullopt;

    const auto number = [ts](std::size_t pos, std::size_t len, int& value) {
        const char* first = ts.data() + pos;
        const auto  res   = std::from_chars(first, first + len, value);
        return res.ec == std::errc{} && res.ptr == first + len;
    };
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    if (!number(0, 4, y) || !number(5, 2, mo) || !number(8, 2, d) ||
        !number(11, 2, h) || !number(14, 2, mi) || !number(17, 2, s))
        return std::nullopt;

    const std::chrono::year_month_day ymd{std::chrono::year{y},
                                          std::chrono::month{static_cast<unsigned>(mo)},
                                          std::chrono::day{static_cast<unsigned>(d)}};
    if (!ymd.ok())
        return std::nullopt;

    std::int64_t micros = 0;
    if (ts.size() > 19 && ts[19] == '.')
    {
        int digits = 0;
        for (std::size_t i = 20; i < ts.size() && std::isdigit(static_cast<unsigned char>(ts[i])); ++i)
        {
            if (digits < 6)
            {
                micros = micros * 10 + (ts[i] - '0');
                ++digits;
            }
        }
        for (; digits < 6; ++digits)
            micros *= 10;
    }

    const std::int64_t days = std::chrono::sys_days{ymd}.time_since_epoch().count();
    return ((days * 24 + h) * 60 + mi) * 60'000'000LL + s * 1'000'000LL + micros;
}

namespace
{

/// True for "TestStep 3: …" / "TestStep: …" given the prefix "TestStep".
bool IsMarker(std::string_view text, std::string_view prefix)
{
    if (prefix.empty() || !text.starts_with(prefix))
        return false;
    if (text.size() == prefix.size())
        return true;
    const char next = text[prefix.size()];
    return next == ' ' || next == ':' || std::isdigit(static_cast<unsigned char>(next));
}

std::string ToUpper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

struct Keyword
{
    std::size_t row {0};
    std::string name;
    std::string args;
    bool        failed {false};
};

struct Marker
{
    std::size_t row {0};
    bool        step {false}; ///< TestStep (else TestExpectation)
    std::string text;
};

struct Failure
{
    std::size_t row {0};
    std::string name;
    std::string message;
    int         keyword {-1}; ///< index into the keywords, -1 = no matching STEP
    bool        recovered {false};
};

} // namespace

TestOutline BuildTestOutline(db::EventsContainer& events, const TestMarkerRules& r)
{
    TestOutline out;

    for (const auto& [key, value] : events.GetFileMetadata())
    {
        if (key == kMetaTestCase)
            out.testName = value;
        else if (key == kMetaStatus)
        {
            const std::string status = ToUpper(value);
            out.verdict = status == "PASS" ? TestVerdict::Pass
                        : status == "FAIL" ? TestVerdict::Fail
                                           : TestVerdict::Unknown;
        }
    }

    const std::size_t n = events.Size();
    std::size_t       start = 0;
    while (start < n && events.GetEvent(start).findByKey(r.typeField) != r.testStart)
        ++start;
    if (start == n)
        return out;
    out.testStartRow = start;

    if (out.testName.empty())
    {
        std::string name = events.GetEvent(start).findByKey(r.textField);
        if (name.starts_with("name="))
            name.erase(0, 5);
        out.testName = std::move(name);
    }

    // ── One pass over the test: keywords, markers, failures, commands ──────
    std::vector<Keyword>     keywords;
    std::vector<Marker>      markers;
    std::vector<Failure>     failures;
    std::vector<std::size_t> commandRows;
    std::size_t              stop = n; // one past the test's last event
    for (std::size_t i = start + 1; i < n; ++i)
    {
        const db::LogEvent& ev   = events.GetEvent(i);
        const std::string   type = ev.findByKey(r.typeField);
        if (type == r.testEnd)
        {
            out.testEndRow = i;
            stop           = i;
            if (out.verdict == TestVerdict::Unknown)
            {
                const std::string text = ev.findByKey(r.nameField);
                if (text.find("(FAIL)") != std::string::npos)
                    out.verdict = TestVerdict::Fail;
                else if (text.find("(PASS)") != std::string::npos)
                    out.verdict = TestVerdict::Pass;
            }
            break;
        }
        if (type == r.testStart) // the next test begins without an end
        {
            stop = i;
            break;
        }
        if (type == r.keyword)
            keywords.push_back({i, ev.findByKey(r.nameField), ev.findByKey(r.textField)});
        else if (type == r.keywordFail)
            failures.push_back({i, ev.findByKey(r.nameField), ev.findByKey(r.textField)});
        else if (type == r.log)
        {
            std::string text = ev.findByKey(r.textField);
            if (IsMarker(text, r.stepMarker))
                markers.push_back({i, true, std::move(text)});
            else if (IsMarker(text, r.expectationMarker))
                markers.push_back({i, false, std::move(text)});
        }
        else if (ev.findByKey(r.nameField) == r.commandName)
            commandRows.push_back(i);
    }

    // ── Failures: owning keyword, recovered or not, the decisive one ───────
    std::unordered_map<std::string, std::size_t> lastStart; // name + args → last start row
    const auto keyOf = [](const Keyword& k) { return k.name + '\x1f' + k.args; };
    for (const auto& k : keywords)
        lastStart[keyOf(k)] = k.row;

    std::optional<std::size_t> decisive; // index into failures
    for (std::size_t f = 0; f < failures.size(); ++f)
    {
        Failure& fail = failures[f];
        const auto end = std::upper_bound(keywords.begin(), keywords.end(), fail.row,
            [](std::size_t row, const Keyword& k) { return row < k.row; });
        for (auto it = std::make_reverse_iterator(end); it != keywords.rend(); ++it)
        {
            if (!it->failed && it->name == fail.name)
            {
                it->failed   = true;
                fail.keyword = static_cast<int>(std::distance(keywords.begin(), it.base()) - 1);
                break;
            }
        }
        fail.recovered = out.verdict == TestVerdict::Pass ||
                         (fail.keyword >= 0 &&
                          lastStart[keyOf(keywords[static_cast<std::size_t>(fail.keyword)])] > fail.row);
        if (!fail.recovered && !decisive && out.verdict == TestVerdict::Fail)
            decisive = f;
    }

    if (decisive)
    {
        out.decisiveFailureRow = failures[*decisive].row;
        out.failingStep        = failures[*decisive].name;
        out.failureMessage     = failures[*decisive].message;
    }
    else if (out.verdict == TestVerdict::Fail)
    {
        out.decisiveFailureRow = out.testEndRow;
    }
    // Steps after this row may not have run (only after a recorded failure).
    const std::size_t notRunAfter = decisive ? failures[*decisive].row : n;

    // ── Spans, times and commands ──────────────────────────────────────────
    std::vector<std::size_t> boundaries; // rows where a step starts
    boundaries.reserve(keywords.size() + markers.size());
    for (const auto& k : keywords)
        boundaries.push_back(k.row);
    for (const auto& m : markers)
        boundaries.push_back(m.row);
    std::sort(boundaries.begin(), boundaries.end());

    const auto nextBoundary = [&](std::size_t row) {
        const auto it = std::upper_bound(boundaries.begin(), boundaries.end(), row);
        return it == boundaries.end() ? stop : *it;
    };
    const auto timeAt = [&](std::size_t row) -> std::optional<std::int64_t> {
        if (row >= n)
            row = stop - 1; // log ends inside the test: last event
        return ParseIsoMicros(events.GetEvent(row).findByKey(r.timeField));
    };
    const auto t0 = timeAt(start);
    const auto relative = [&](std::size_t row) -> std::int64_t {
        const auto t = timeAt(row);
        return t && t0 ? *t - *t0 : -1;
    };
    const auto duration = [&](std::size_t from, std::size_t to) -> std::int64_t {
        const auto a = timeAt(from);
        const auto b = timeAt(to);
        return a && b ? *b - *a : -1;
    };
    const auto hasCommand = [&](std::size_t from, std::size_t to) {
        const auto it = std::lower_bound(commandRows.begin(), commandRows.end(), from);
        return it != commandRows.end() && *it < to;
    };

    std::unordered_set<std::string> commandSenders; // keywords seen sending a command
    for (const auto& k : keywords)
        if (hasCommand(k.row, nextBoundary(k.row)))
            commandSenders.insert(k.name);

    // ── Leaf steps ─────────────────────────────────────────────────────────
    std::vector<TestStep> leaves; // keywords (same index as `keywords`), then expectations
    leaves.reserve(keywords.size() + markers.size());
    for (const auto& kw : keywords)
    {
        TestStep step;
        step.kind       = TestStep::Kind::Keyword;
        step.name       = kw.name;
        step.args       = kw.args;
        step.firstRow   = kw.row;
        step.endRow     = nextBoundary(kw.row);
        step.startUs    = relative(kw.row);
        step.durationUs = duration(kw.row, step.endRow);
        if (kw.row > notRunAfter && !kw.failed && !hasCommand(kw.row, step.endRow) &&
            commandSenders.contains(kw.name))
            step.status = StepStatus::NotRun;
        leaves.push_back(std::move(step));
    }
    for (const auto& fail : failures)
    {
        if (fail.keyword < 0)
            continue;
        TestStep& step   = leaves[static_cast<std::size_t>(fail.keyword)];
        step.failures    = 1;
        step.failMessage = fail.message;
        step.status      = fail.recovered ? StepStatus::Recovered : StepStatus::Failed;
    }
    for (const auto& m : markers)
    {
        if (m.step)
            continue;
        TestStep step;
        step.kind     = TestStep::Kind::Expectation;
        step.name     = m.text;
        step.firstRow = m.row;
        step.endRow   = nextBoundary(m.row);
        step.startUs  = relative(m.row);
        if (m.row > notRunAfter)
            step.status = StepStatus::NotRun;
        leaves.push_back(std::move(step));
    }
    std::sort(leaves.begin(), leaves.end(),
              [](const TestStep& a, const TestStep& b) { return a.firstRow < b.firstRow; });

    // ── Sections ───────────────────────────────────────────────────────────
    struct SectionStart
    {
        std::size_t row;
        std::string title;
    };
    const bool hasStepMarkers =
        std::any_of(markers.begin(), markers.end(), [](const Marker& m) { return m.step; });
    std::vector<SectionStart> starts{{start, hasStepMarkers ? "Setup" : "Test"}};
    for (const auto& m : markers)
        if (m.step)
            starts.push_back({m.row, m.text});
    if (!markers.empty() && !markers.back().step)
    {
        const std::size_t lastExpectation = markers.back().row;
        const auto it = std::find_if(keywords.begin(), keywords.end(),
            [&](const Keyword& k) { return k.row > lastExpectation; });
        if (it != keywords.end())
            starts.push_back({it->row, "Teardown"});
    }

    auto leaf = leaves.begin();
    for (std::size_t s = 0; s < starts.size(); ++s)
    {
        TestStep section;
        section.kind       = TestStep::Kind::Section;
        section.name       = starts[s].title;
        section.firstRow   = starts[s].row;
        section.endRow     = s + 1 < starts.size() ? starts[s + 1].row : stop;
        section.startUs    = relative(section.firstRow);
        section.durationUs = duration(section.firstRow, section.endRow);

        for (; leaf != leaves.end() && leaf->firstRow < section.endRow; ++leaf)
            section.children.push_back(std::move(*leaf));

        bool failed = false;
        for (const auto& child : section.children)
        {
            section.failures += child.failures;
            if (!child.failMessage.empty())
                section.failMessage = child.failMessage;
            failed = failed || child.status == StepStatus::Failed;
        }
        for (const auto& fail : failures) // failures without a matching STEP
        {
            if (fail.keyword < 0 && fail.row >= section.firstRow && fail.row < section.endRow)
            {
                ++section.failures;
                section.failMessage = fail.message;
                failed = failed || !fail.recovered;
            }
        }

        if (section.firstRow > notRunAfter && section.failures == 0 &&
            !hasCommand(section.firstRow, section.endRow))
        {
            section.status = StepStatus::NotRun;
            for (auto& child : section.children)
                child.status = StepStatus::NotRun;
        }
        else if (failed)
            section.status = StepStatus::Failed;
        else if (section.failures > 0)
            section.status = StepStatus::Recovered;

        // An empty Setup (a TestStep marker right after TEST_START) says nothing.
        if (s == 0 && hasStepMarkers && section.children.empty() && section.failures == 0)
            continue;
        out.sections.push_back(std::move(section));
    }
    return out;
}

} // namespace analyzer
