#include "SapiLogParser.hpp"

#include "Error.hpp"
#include "LogEvent.hpp"
#include "Logger.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
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
constexpr size_t           kSniffBytes       = 4096;
constexpr std::string_view kUtf8Bom          = "\xEF\xBB\xBF";

// Payload field extraction bounds (memory stays bounded per event).
constexpr std::string_view kPayloadPrefix       = "p.";
constexpr size_t           kMaxPayloadLength    = 4096;
constexpr size_t           kMaxPayloadFields    = 32;
constexpr size_t           kMaxFieldKeyLength   = 64;
constexpr size_t           kMaxFieldValueLength = 512;

using Pairs = std::vector<std::pair<std::string, std::string>>;

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

bool IsBlank(char c) { return c == ' ' || c == '\t'; }

/// Splits @p s at whitespace outside `[...]`/`{...}` and outside a quoted
/// value (a quote right after '='). False on unbalanced brackets or an
/// unterminated quote.
bool SplitTokens(std::string_view s, std::vector<std::string_view>& tokens)
{
    size_t i = 0;
    while (i < s.size())
    {
        while (i < s.size() && IsBlank(s[i]))
            ++i;
        if (i >= s.size())
            break;

        const size_t start = i;
        int          depth = 0;
        char         quote = 0;
        for (; i < s.size(); ++i)
        {
            const char c = s[i];
            if (quote != 0)
            {
                if (c == '\\')
                    ++i;
                else if (c == quote)
                    quote = 0;
            }
            else if ((c == '"' || c == '\'') && depth == 0 && i > start && s[i - 1] == '=')
                quote = c;
            else if (c == '[' || c == '{')
                ++depth;
            else if (c == ']' || c == '}')
            {
                if (--depth < 0)
                    return false;
            }
            else if (depth == 0 && IsBlank(c))
                break;
        }
        if (depth != 0 || quote != 0)
            return false;
        tokens.push_back(s.substr(start, i - start));
    }
    return true;
}

bool IsValidKey(std::string_view key)
{
    if (key.empty() || !(std::isalpha(static_cast<unsigned char>(key[0])) || key[0] == '_'))
        return false;
    return std::all_of(key.begin(), key.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '-';
    });
}

/// Parses one `key=value` token; a value wrapped in matching quotes is unquoted
/// (backslash escapes the next character).
bool ParseKeyValue(std::string_view token, Pairs& pairs)
{
    const size_t eq = token.find('=');
    if (eq == std::string_view::npos || !IsValidKey(token.substr(0, eq)))
        return false;
    std::string_view value = token.substr(eq + 1);
    if (value.empty())
        return false;

    std::string text;
    if (value.front() == '"' || value.front() == '\'')
    {
        // SplitTokens guarantees the quote is closed; it must end the token.
        size_t j = 1;
        for (; j < value.size() && value[j] != value.front(); ++j)
        {
            if (value[j] == '\\' && j + 1 < value.size())
                ++j;
            text += value[j];
        }
        if (j != value.size() - 1)
            return false;
    }
    else
    {
        text = value;
    }
    pairs.emplace_back(std::string{token.substr(0, eq)}, std::move(text));
    return true;
}

/// `key=value` tokens, optionally inside bracket groups (one level). A group
/// holding a single word without '=' is a label (`[P0]`) and is skipped.
bool ExtractKeyValues(std::string_view payload, Pairs& pairs)
{
    std::vector<std::string_view> tokens;
    if (!SplitTokens(payload, tokens))
        return false;

    for (const auto token : tokens)
    {
        if (token.front() != '[')
        {
            if (!ParseKeyValue(token, pairs))
                return false;
            continue;
        }
        if (token.back() != ']')
            return false;

        std::vector<std::string_view> inner;
        if (!SplitTokens(token.substr(1, token.size() - 2), inner))
            return false;
        if (inner.size() == 1 && inner[0].find_first_of("=[{") == std::string_view::npos)
            continue; // label
        for (const auto t : inner)
        {
            if (t.front() == '[' || !ParseKeyValue(t, pairs))
                return false;
        }
    }
    return !pairs.empty();
}

std::string JsonText(const nlohmann::ordered_json& v)
{
    if (v.is_string())
        return v.get<std::string>();
    return v.dump(-1, ' ', false, nlohmann::ordered_json::error_handler_t::replace);
}

/// Flat or one-level-deep JSON object; deeper values stay compact JSON text.
bool ExtractJson(std::string_view payload, Pairs& pairs)
{
    const auto doc = nlohmann::ordered_json::parse(payload, nullptr, /*allow_exceptions=*/false);
    if (!doc.is_object())
        return false; // also covers a parse failure (discarded value)

    for (const auto& [key, value] : doc.items())
    {
        if (!value.is_object())
        {
            pairs.emplace_back(key, JsonText(value));
            continue;
        }
        for (const auto& [member, inner] : value.items())
            pairs.emplace_back(key + "." + member, JsonText(inner));
    }
    return true;
}

} // namespace

void SapiLogParser::ParseHeaderLine(std::string_view line, std::vector<std::pair<std::string, std::string>>& metadata)
{
    line.remove_prefix(1); // '#'
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos)
        return;
    const std::string_view key = Trim(line.substr(0, colon));
    const bool keyIsWords = !key.empty() && std::all_of(key.begin(), key.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '_' || c == '-';
    });
    if (keyIsWords)
        metadata.emplace_back(std::string{key}, std::string{Trim(line.substr(colon + 1))});
}

std::string_view SapiLogParser::ExtractUnit(std::string_view info)
{
    size_t           pos = 0;
    std::string_view marker;
    if (info.empty() || info.front() != '[' || !ReadGroup(info, pos, marker))
        return {};

    const size_t bar = marker.find('|');
    if (bar == std::string_view::npos)
        return {};
    const std::string_view unit      = Trim(marker.substr(0, bar));
    const std::string_view partition = Trim(marker.substr(bar + 1));
    const auto isWord = [](std::string_view w) {
        return !w.empty() && w.find_first_of(" \t|[]") == std::string_view::npos;
    };
    return isWord(unit) && isWord(partition) ? unit : std::string_view{};
}

void SapiLogParser::ExtractPayloadFields(std::string_view payload, db::LogEvent::EventItems& out)
{
    payload = Trim(payload);
    if (payload.empty() || payload.size() > kMaxPayloadLength)
        return;

    Pairs      pairs;
    const bool ok = payload.front() == '{' && payload.back() == '}'
                        ? ExtractJson(payload, pairs)
                        : ExtractKeyValues(payload, pairs);
    if (!ok)
        return;

    const size_t first = out.size();
    for (auto& [key, value] : pairs)
    {
        if (out.size() - first >= kMaxPayloadFields)
            break;
        if (key.empty() || key.size() > kMaxFieldKeyLength || value.size() > kMaxFieldValueLength)
            continue;

        std::string name = std::string{kPayloadPrefix} + key;
        const bool  seen = std::any_of(out.begin() + static_cast<std::ptrdiff_t>(first), out.end(),
            [&](const auto& item) { return item.first == name; });
        if (!seen)
            out.emplace_back(std::move(name), std::move(value));
    }
}

bool SapiLogParser::LooksLikeSapiLog(const std::filesystem::path& filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        return false;

    // Read a bounded prefix: a .txt without newlines (minified blob, binary)
    // must not be slurped whole just to be sniffed.
    std::string prefix(kSniffBytes, '\0');
    file.read(prefix.data(), static_cast<std::streamsize>(prefix.size()));
    prefix.resize(static_cast<size_t>(file.gcount()));

    std::string_view rest{prefix};
    if (rest.substr(0, kUtf8Bom.size()) == kUtf8Bom)
        rest.remove_prefix(kUtf8Bom.size());

    for (int i = 0; i < kHeaderProbeLines && !rest.empty(); ++i)
    {
        if (rest.substr(0, kHeaderMarker.size()) == kHeaderMarker)
            return true;
        const size_t eol = rest.find('\n');
        if (eol == std::string_view::npos)
            break;
        rest.remove_prefix(eol + 1);
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

    const std::string_view   unit = ExtractUnit(fields[6]);
    db::LogEvent::EventItems extra;
    ExtractPayloadFields(payload, extra);

    out.reserve(out.size() + 8 + (unit.empty() ? 0 : 1) + extra.size());
    out.emplace_back("timestamp",   std::string{fields[0]});
    out.emplace_back("category",    std::string{fields[1]});
    out.emplace_back("source",      std::string{fields[2]});
    out.emplace_back("destination", std::string{fields[3]});
    out.emplace_back("level",       std::string{fields[4]});
    out.emplace_back("event_type",  std::string{fields[5]});
    out.emplace_back("info",        std::string{fields[6]});
    if (!payload.empty())
        out.emplace_back("payload", std::string{payload});
    if (!unit.empty())
        out.emplace_back("unit", std::string{unit});
    std::move(extra.begin(), extra.end(), std::back_inserter(out));
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

    m_metadata.clear();

    std::string line;
    int         id        = 0;
    size_t      dataLines = 0;
    size_t      skipped   = 0;

    bool firstLine = true;
    while (std::getline(input, line))
    {
        m_currentProgress += static_cast<uint32_t>(line.size() + 1);

        std::string_view content{line};
        if (firstLine)
        {
            firstLine = false;
            if (content.substr(0, kUtf8Bom.size()) == kUtf8Bom)
                content.remove_prefix(kUtf8Bom.size()); // else the first event is lost
        }

        const std::string_view trimmed = Trim(content);
        if (trimmed.empty())
            continue;
        if (trimmed.front() == '#')
        {
            if (dataLines == 0) // header block only; later comments are ignored
                ParseHeaderLine(trimmed, m_metadata);
            continue;
        }
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

    if (input.bad())
    {
        throw error::Error(error::ErrorCode::ParseError,
            "SapiLogParser: read error, the file may be truncated");
    }

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
