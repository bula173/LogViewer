#include "TestRunIndex.hpp"

#include "EventsContainer.hpp"
#include "sapi/SapiLogParser.hpp"

#include <QFile>
#include <QString>
#include <QXmlStreamReader>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <system_error>
#include <utility>

namespace analyzer
{

namespace
{

namespace fs = std::filesystem;

using Metadata = db::EventsContainer::FileMetadata;

constexpr int               kMaxHeaderLines = 64;        // the real header has 9
constexpr std::streamoff    kTailBytes      = 64 * 1024; // holds the last event line
constexpr std::string_view  kUtf8Bom        = "\xEF\xBB\xBF";

std::string_view Trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

/// Reads the `#` header block of @p in; @p firstDataLine receives the first
/// line after it (empty at end of file). The lines are decoded by
/// SapiLogParser, so the keys match a loaded log's file metadata.
Metadata ReadHeader(std::istream& in, std::string& firstDataLine)
{
    Metadata    metadata;
    std::string line;
    firstDataLine.clear();
    for (int i = 0; i < kMaxHeaderLines && std::getline(in, line); ++i)
    {
        std::string_view content{line};
        if (i == 0 && content.starts_with(kUtf8Bom))
            content.remove_prefix(kUtf8Bom.size());
        content = Trim(content);
        if (content.empty())
            continue;
        if (content.front() != '#')
        {
            firstDataLine = std::string{content}; // without a BOM
            break;
        }
        parser::SapiLogParser::ParseHeaderLine(content, metadata);
    }
    return metadata;
}

std::string Field(const db::LogEvent::EventItems& items, std::string_view key)
{
    const auto it = std::find_if(items.begin(), items.end(),
                                 [key](const auto& item) { return item.first == key; });
    return it == items.end() ? std::string{} : it->second;
}

std::string EventTimestamp(std::string_view line)
{
    db::LogEvent::EventItems items;
    return parser::SapiLogParser::ParseLine(line, items) ? Field(items, "timestamp") : std::string{};
}

/// Timestamp of the last event line, read from the end of the file.
std::string LastEventTimestamp(const fs::path& file)
{
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in)
        return {};
    const std::streamoff size  = in.tellg();
    const std::streamoff start = std::max<std::streamoff>(0, size - kTailBytes);
    std::string tail(static_cast<std::size_t>(size - start), '\0');
    in.seekg(start);
    in.read(tail.data(), static_cast<std::streamsize>(tail.size()));
    tail.resize(static_cast<std::size_t>(in.gcount()));

    std::string_view rest{tail};
    while (!rest.empty())
    {
        const std::size_t eol  = rest.find_last_of('\n', rest.size() - 1);
        const std::size_t from = eol == std::string_view::npos ? 0 : eol + 1;
        if (std::string ts = EventTimestamp(rest.substr(from)); !ts.empty())
            return ts;
        if (eol == std::string_view::npos)
            break;
        rest = rest.substr(0, eol);
    }
    return {};
}

std::string FileTestName(const fs::path& file)
{
    std::string name = file.filename().string();
    if (name.ends_with(kMergedLogSuffix))
        name.resize(name.size() - kMergedLogSuffix.size());
    std::replace(name.begin(), name.end(), '_', ' ');
    return name;
}

std::string ToUpper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

void AddFailure(std::unordered_map<std::string, std::string>& failures,
                const QString& testName, const QString& message)
{
    if (!testName.isEmpty() && !message.isEmpty())
        failures.emplace(TestNameKey(testName.toStdString()), CollapseWhitespace(message.toStdString()));
}

/// xunit.xml: `<testcase name=…>` holding `<failure message=…>` / `<error message=…>`.
bool ReadXunitFailures(const fs::path& path, std::unordered_map<std::string, std::string>& failures)
{
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QXmlStreamReader xml(&file);
    QString          testName;
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement())
        {
            if (xml.name() == u"testcase")
                testName = xml.attributes().value(QStringLiteral("name")).toString();
            else if (xml.name() == u"failure" || xml.name() == u"error")
            {
                QString message = xml.attributes().value(QStringLiteral("message")).toString();
                if (message.isEmpty())
                    message = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                AddFailure(failures, testName, message);
            }
        }
        else if (xml.isEndElement() && xml.name() == u"testcase")
        {
            testName.clear();
        }
    }
    return !xml.hasError();
}

/// output.xml: the text of the FAIL `<status>` that is a direct child of a
/// `<test name=…>` (keyword statuses are nested deeper).
void ReadOutputXmlFailures(const fs::path& path, std::unordered_map<std::string, std::string>& failures)
{
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly))
        return;

    QXmlStreamReader xml(&file);
    QString          testName;
    int              depth     = 0;
    int              testDepth = -1;
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement())
        {
            ++depth;
            if (xml.name() == u"test")
            {
                testName  = xml.attributes().value(QStringLiteral("name")).toString();
                testDepth = depth;
            }
            else if (xml.name() == u"status" && depth == testDepth + 1 &&
                     xml.attributes().value(QStringLiteral("status")) == u"FAIL")
            {
                AddFailure(failures, testName, xml.readElementText());
                --depth; // readElementText() consumed the end element
            }
        }
        else if (xml.isEndElement())
        {
            if (depth == testDepth)
                testDepth = -1;
            --depth;
        }
    }
}

} // namespace

TestRunEntry ReadTestLogSummary(const fs::path& file)
{
    TestRunEntry entry;
    entry.file = file;

    std::string firstTs;
    {
        std::ifstream in(file, std::ios::binary);
        std::string   firstLine;
        for (const auto& [key, value] : ReadHeader(in, firstLine))
        {
            if (key == kMetaTestCase)
                entry.testName = value;
            else if (key == kMetaStatus)
            {
                const std::string status = ToUpper(value);
                entry.verdict = status == "PASS" ? TestVerdict::Pass
                              : status == "FAIL" ? TestVerdict::Fail
                                                 : TestVerdict::Unknown;
            }
            else if (key == "Timestamp")
                entry.timestamp = value;
            else if (key == "Entries")
            {
                std::int64_t n = 0;
                const auto   res = std::from_chars(value.data(), value.data() + value.size(), n);
                if (res.ec == std::errc{} && res.ptr == value.data() + value.size())
                    entry.entries = n;
            }
        }
        firstTs = EventTimestamp(firstLine);
    }

    if (entry.testName.empty())
        entry.testName = FileTestName(file);
    if (entry.timestamp.empty())
        entry.timestamp = firstTs;

    const auto first = ParseIsoMicros(firstTs);
    const auto last  = ParseIsoMicros(LastEventTimestamp(file));
    if (first && last && *last >= *first)
        entry.durationUs = *last - *first;
    return entry;
}

std::string FindDecisiveFailureMessage(const fs::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return {};

    const TestMarkerRules rules;
    const std::array<std::string, 4> wanted{rules.testStart, rules.testEnd, rules.keyword, rules.keywordFail};
    std::array<std::string, 4> tokens; // "[STEP]" … as they appear on a line
    std::transform(wanted.begin(), wanted.end(), tokens.begin(),
                   [](const std::string& type) { return '[' + type + ']'; });

    std::string line;
    Metadata    metadata = ReadHeader(in, line);

    std::vector<std::pair<int, db::LogEvent::EventItems>> markers;
    int  id   = 0;
    bool more = !line.empty();
    while (more)
    {
        const bool candidate = std::any_of(tokens.begin(), tokens.end(), [&line](const std::string& t) {
            return line.find(t) != std::string::npos;
        });
        db::LogEvent::EventItems items;
        if (candidate && parser::SapiLogParser::ParseLine(line, items) &&
            std::find(wanted.begin(), wanted.end(), Field(items, rules.typeField)) != wanted.end())
        {
            markers.emplace_back(++id, std::move(items));
        }
        more = static_cast<bool>(std::getline(in, line));
    }

    db::EventsContainer events;
    events.AddEventBatch(std::move(markers));
    events.SetFileMetadata(std::move(metadata));
    const TestOutline outline = BuildTestOutline(events, rules);
    return outline.failingStep.empty() ? std::string{} : CollapseWhitespace(outline.failureMessage);
}

std::unordered_map<std::string, std::string> ReadRobotFailures(const fs::path& folder)
{
    std::unordered_map<std::string, std::string> failures;
    std::error_code ec;
    if (fs::exists(folder / "xunit.xml", ec) && ReadXunitFailures(folder / "xunit.xml", failures))
        return failures;
    failures.clear();
    ReadOutputXmlFailures(folder / "output.xml", failures);
    return failures;
}

std::string TestNameKey(std::string_view name)
{
    std::string key;
    for (const char c : name)
        if (std::isalnum(static_cast<unsigned char>(c)))
            key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return key;
}

std::string CollapseWhitespace(std::string_view text)
{
    std::string out;
    bool        space = false;
    for (const char c : Trim(text))
    {
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            space = true;
            continue;
        }
        if (space)
            out.push_back(' ');
        space = false;
        out.push_back(c);
    }
    return out;
}

std::string NormalizeFailureMessage(std::string_view message)
{
    std::string out;
    bool        inDigits = false;
    for (const char c : CollapseWhitespace(message))
    {
        const bool digit = std::isdigit(static_cast<unsigned char>(c)) != 0;
        if (!digit)
            out.push_back(c);
        else if (!inDigits)
            out.push_back('N');
        inDigits = digit;
    }
    return out;
}

std::vector<TestRunEntry> ScanResultsFolder(const fs::path& folder, const ScanProgress& progress)
{
    std::vector<fs::path> files;
    std::error_code       ec;
    for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec))
    {
        if (it->is_regular_file(ec) && it->path().filename().string().ends_with(kMergedLogSuffix))
            files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());

    std::vector<TestRunEntry> entries;
    entries.reserve(files.size());
    bool needRobotXml = false;
    for (const auto& file : files)
    {
        TestRunEntry entry = ReadTestLogSummary(file);
        if (entry.verdict == TestVerdict::Fail)
        {
            entry.failureMessage = FindDecisiveFailureMessage(file);
            if (!entry.failureMessage.empty())
                entry.failureSource = FailureSource::StepFail;
            else
                needRobotXml = true;
        }
        entries.push_back(std::move(entry));
        if (progress && !progress(entries.size(), files.size()))
            return {};
    }

    if (needRobotXml)
    {
        const auto failures = ReadRobotFailures(folder);
        for (auto& entry : entries)
        {
            if (entry.verdict != TestVerdict::Fail || !entry.failureMessage.empty())
                continue;
            if (const auto it = failures.find(TestNameKey(entry.testName)); it != failures.end())
            {
                entry.failureMessage = it->second;
                entry.failureSource  = FailureSource::RobotXml;
            }
        }
    }

    std::stable_sort(entries.begin(), entries.end(), [](const TestRunEntry& a, const TestRunEntry& b) {
        return a.timestamp < b.timestamp; // files are already sorted by name
    });
    return entries;
}

std::vector<FailureGroup> GroupFailures(const std::vector<TestRunEntry>& entries)
{
    std::vector<FailureGroup>                    groups;
    std::unordered_map<std::string, std::size_t> bySignature;
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const TestRunEntry& entry = entries[i];
        if (entry.verdict != TestVerdict::Fail || entry.failureMessage.empty())
            continue;
        const auto [it, added] = bySignature.emplace(NormalizeFailureMessage(entry.failureMessage), groups.size());
        if (added)
            groups.push_back({entry.failureMessage, {}});
        groups[it->second].members.push_back(i);
    }
    std::stable_sort(groups.begin(), groups.end(), [](const FailureGroup& a, const FailureGroup& b) {
        return a.members.size() > b.members.size();
    });
    return groups;
}

} // namespace analyzer
