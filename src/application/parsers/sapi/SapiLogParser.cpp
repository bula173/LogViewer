#include "SapiLogParser.hpp"

#include "Error.hpp"
#include "LogEvent.hpp"
#include "Logger.hpp"

#include <cctype>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace parser
{

namespace
{

constexpr size_t           kBatchSize    = 5000;
constexpr size_t           kFixedFields  = 7; // timestamp … info; payload is the remainder
constexpr std::string_view kHeaderMarker = "# FULL MERGED TEST STEPS";
constexpr int              kHeaderProbeLines = 5;

/// Reads the balanced `[...]` group starting at line[pos] (which must be '[').
/// On success @p content is the text between the outer brackets and @p pos
/// moves past the closing ']'. Returns false when the group never closes.
bool ReadGroup(std::string_view line, size_t& pos, std::string_view& content)
{
    int depth = 0;
    for (size_t j = pos; j < line.size(); ++j)
    {
        if (line[j] == '[')
        {
            ++depth;
        }
        else if (line[j] == ']' && --depth == 0)
        {
            content = line.substr(pos + 1, j - pos - 1);
            pos     = j + 1;
            return true;
        }
    }
    return false;
}

std::string_view Trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.remove_suffix(1);
    return s;
}

/// Cheap sanity check that a field looks like "YYYY-MM-DD…".
bool LooksLikeIsoTimestamp(std::string_view ts)
{
    return ts.size() >= 10 &&
           std::isdigit(static_cast<unsigned char>(ts[0])) &&
           ts[4] == '-' && ts[7] == '-';
}

} // namespace

bool SapiLogParser::LooksLikeSapiLog(const std::filesystem::path& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
        return false;

    std::string line;
    for (int i = 0; i < kHeaderProbeLines && std::getline(file, line); ++i)
    {
        if (std::string_view{line}.substr(0, kHeaderMarker.size()) == kHeaderMarker)
            return true;
    }
    return false;
}

bool SapiLogParser::ParseLine(std::string_view line, db::LogEvent::EventItems& out)
{
    line = Trim(line);
    if (line.empty() || line.front() == '#')
        return false;

    std::string_view fields[kFixedFields];
    size_t           pos = 0;
    for (size_t k = 0; k < kFixedFields; ++k)
    {
        while (pos < line.size() && line[pos] == ' ')
            ++pos;
        if (pos >= line.size() || line[pos] != '[')
            return false;

        if (!ReadGroup(line, pos, fields[k]))
        {
            // An unclosed bracket in the free-text `info` field swallows the
            // rest of the line; every earlier field is machine-generated and
            // must be well-formed.
            if (k != kFixedFields - 1)
                return false;
            fields[k] = line.substr(pos + 1);
            pos       = line.size();
        }
    }

    if (!LooksLikeIsoTimestamp(fields[0]))
        return false;

    // Payload: a single bracket group is unwrapped, anything else (several
    // groups, trailing text) is kept verbatim.
    std::string_view payload = Trim(line.substr(pos));
    if (!payload.empty() && payload.front() == '[')
    {
        size_t           p = 0;
        std::string_view inner;
        if (ReadGroup(payload, p, inner) && p == payload.size())
            payload = inner;
    }

    out.reserve(8);
    out.emplace_back("timestamp",   std::string{fields[0]});
    out.emplace_back("category",    std::string{fields[1]});
    out.emplace_back("source",      std::string{fields[2]});
    out.emplace_back("destination", std::string{fields[3]});
    out.emplace_back("level",       std::string{fields[4]});
    out.emplace_back("event_type",  std::string{fields[5]});
    out.emplace_back("info",        std::string{fields[6]});
    if (!payload.empty())
        out.emplace_back("payload", std::string{payload});
    return true;
}

void SapiLogParser::ParseData(const std::filesystem::path& filepath)
{
    util::Logger::Debug("SapiLogParser::ParseData opening '{}'", filepath.string());

    std::error_code ec;
    const auto      size = std::filesystem::file_size(filepath, ec);
    m_totalProgress      = ec ? 0u : static_cast<uint32_t>(size);
    m_currentProgress    = 0;

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        throw error::Error(error::ErrorCode::FileNotFound,
            "SapiLogParser: cannot open file: " + filepath.string());
    }

    ParseStream(file);
}

void SapiLogParser::ParseData(std::istream& input)
{
    m_totalProgress   = 0;
    m_currentProgress = 0;
    ParseStream(input);
}

void SapiLogParser::ParseStream(std::istream& input)
{
    std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
    batch.reserve(kBatchSize);

    auto flush = [&]() {
        if (batch.empty())
            return;
        NotifyNewEventBatch(std::move(batch));
        NotifyProgressUpdated();
        batch.clear();
        batch.reserve(kBatchSize);
    };

    std::string line;
    int         id        = 0;
    size_t      dataLines = 0;
    size_t      skipped   = 0;

    while (std::getline(input, line))
    {
        m_currentProgress += static_cast<uint32_t>(line.size() + 1);

        const std::string_view trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#')
            continue;
        ++dataLines;

        db::LogEvent::EventItems items;
        if (!ParseLine(trimmed, items))
        {
            ++skipped;
            util::Logger::Trace("SapiLogParser: skipping malformed line: {}", trimmed);
            continue;
        }

        batch.emplace_back(++id, std::move(items));
        if (batch.size() >= kBatchSize)
            flush();
    }
    flush();

    if (id == 0 && dataLines > 0)
    {
        throw error::Error(error::ErrorCode::ParseError,
            "SapiLogParser: no line matches the expected "
            "[timestamp] [category] [source] [destination] [level] [event type] [info] [payload] layout");
    }

    if (skipped > 0)
        util::Logger::Warn("SapiLogParser: skipped {} malformed line(s)", skipped);
    util::Logger::Info("SapiLogParser: parsed {} events", id);

    m_currentProgress = m_totalProgress;
    NotifyProgressUpdated();
}

} // namespace parser
