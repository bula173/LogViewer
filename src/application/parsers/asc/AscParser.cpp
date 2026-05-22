#include "AscParser.hpp"
#include "dbc/CanDecoder.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace parser
{

namespace
{

constexpr uint32_t kProgressBatchSize = 512;

// Trim leading/trailing whitespace.
std::string_view Trim(std::string_view sv)
{
    const auto b = sv.find_first_not_of(" \t\r\n");
    if (b == std::string_view::npos) return {};
    return sv.substr(b, sv.find_last_not_of(" \t\r\n") - b + 1);
}

// Split a string_view on whitespace tokens.
std::vector<std::string_view> Split(std::string_view sv)
{
    std::vector<std::string_view> tokens;
    size_t pos = 0;
    while (pos < sv.size())
    {
        const size_t start = sv.find_first_not_of(" \t", pos);
        if (start == std::string_view::npos) break;
        const size_t end = sv.find_first_of(" \t", start);
        tokens.push_back(sv.substr(start,
            end == std::string_view::npos ? std::string_view::npos : end - start));
        pos = (end == std::string_view::npos) ? sv.size() : end;
    }
    return tokens;
}

// Parse hex byte string to uint8_t. Returns 0 on failure.
uint8_t HexByte(std::string_view sv)
{
    uint8_t v = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v, 16);
    return v;
}

// Convert a hex frame-ID token to upper-case hex string and return whether it is extended.
// Input may end with 'x' or 'X' for extended frames.
std::pair<std::string, bool> ParseIdToken(std::string_view token)
{
    bool extended = false;
    if (!token.empty() && (token.back() == 'x' || token.back() == 'X'))
    {
        extended = true;
        token.remove_suffix(1);
    }
    std::string upper(token);
    std::transform(upper.begin(), upper.end(), upper.begin(),
        [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return {upper, extended};
}

// Parse a CAN ID hex string to uint32_t (without the extended bit).
uint32_t IdFromHex(std::string_view sv)
{
    uint32_t v = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v, 16);
    return v;
}

} // namespace

AscParser::AscParser(std::filesystem::path dbcPath)
    : m_dbcPath(std::move(dbcPath))
{}

void AscParser::LoadDbc()
{
    if (m_dbcLoaded || m_dbcPath.empty())
        return;
    m_dbc      = dbc::ParseDbcFile(m_dbcPath);
    m_dbcLoaded = true;
}

void AscParser::ParseData(const std::filesystem::path& filepath)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(filepath, ec);
    m_totalProgress   = ec ? 0u : static_cast<uint32_t>(size);
    m_currentProgress = 0;
    m_eventId         = 0;

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("AscParser: cannot open '" + filepath.string() + "'");

    LoadDbc();
    ParseStream(file);
}

void AscParser::ParseData(std::istream& input)
{
    m_totalProgress   = 0;
    m_currentProgress = 0;
    m_eventId         = 0;
    LoadDbc();
    ParseStream(input);
}

void AscParser::ParseStream(std::istream& input)
{
    std::string line;
    uint32_t    linesRead = 0;

    // Accumulate events in batches for efficiency.
    std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
    batch.reserve(kProgressBatchSize);

    while (std::getline(input, line))
    {
        ++linesRead;
        m_currentProgress += static_cast<uint32_t>(line.size() + 1);

        const std::string_view sv = Trim(line);

        // Skip header/comment lines.
        if (sv.empty() || sv[0] == '/')
            continue;
        if (sv.substr(0, 6) == "date  " || sv.substr(0, 4) == "base" ||
            sv.substr(0, 6) == "Begin " || sv.substr(0, 4) == "End " ||
            sv == "Begin measurement" || sv == "End measurement")
            continue;

        const auto tokens = Split(sv);
        // Minimum: timestamp channel id dir 'd' dlc
        if (tokens.size() < 2)
            continue;

        // Tokens[0] = timestamp (float seconds)
        const std::string_view tsStr = tokens[0];

        // ErrorFrame: "   <ts>  <ch>  ErrorFrame"
        if (tokens.size() >= 3 && tokens[2] == "ErrorFrame")
        {
            db::LogEvent::EventItems items;
            items.reserve(4);
            items.emplace_back("timestamp", std::string{tsStr});
            items.emplace_back("type",      "ErrorFrame");
            items.emplace_back("CAN_Channel", std::string{tokens[1]});
            items.emplace_back("info",      "CAN ErrorFrame");

            batch.emplace_back(m_eventId++, std::move(items));
        }
        // Normal CAN frame: <ts> <ch> <id[X]> <dir> d <dlc> [bytes...]
        else if (tokens.size() >= 6 && tokens[4] == "d")
        {
            const auto [idHex, extended] = ParseIdToken(tokens[2]);
            const uint32_t canId         = IdFromHex(idHex);
            const std::string_view dirStr = tokens[3];
            const std::string_view dlcStr = tokens[5];

            // Collect raw data bytes.
            std::vector<uint8_t> rawData;
            uint32_t dlc = 0;
            std::from_chars(dlcStr.data(), dlcStr.data() + dlcStr.size(), dlc);
            rawData.reserve(dlc);

            std::string hexStr;
            for (size_t i = 6; i < 6 + dlc && i < tokens.size(); ++i)
            {
                rawData.push_back(HexByte(tokens[i]));
                if (!hexStr.empty()) hexStr += ' ';
                hexStr += std::string(tokens[i]);
            }
            std::transform(hexStr.begin(), hexStr.end(), hexStr.begin(),
                [](unsigned char c){ return static_cast<char>(std::toupper(c)); });

            // Determine direction type string.
            std::string typeStr;
            if      (dirStr == "Rx")   typeStr = "Rx";
            else if (dirStr == "Tx")   typeStr = "Tx";
            else if (dirStr == "TxRq") typeStr = "TxRq";
            else                       typeStr = std::string{dirStr};

            db::LogEvent::EventItems items;
            items.reserve(10);
            items.emplace_back("timestamp",   std::string{tsStr});
            items.emplace_back("type",        typeStr);
            items.emplace_back("CAN_Channel", std::string{tokens[1]});
            items.emplace_back("CAN_ID",      idHex);
            items.emplace_back("CAN_IDE",     extended ? "Extended" : "Standard");
            items.emplace_back("CAN_DLC",     std::string{dlcStr});
            items.emplace_back("CAN_Data",    hexStr);

            // DBC decoding.
            if (m_dbcLoaded && !m_dbc.messages.empty())
            {
                const auto msgIt = m_dbc.messages.find(canId);
                if (msgIt != m_dbc.messages.end())
                {
                    items.emplace_back("CAN_MsgName", msgIt->second.name);
                    auto signals = dbc::DecodeFrame(canId, rawData, m_dbc);
                    for (auto& sv2 : signals)
                        items.push_back(std::move(sv2));
                }
            }

            // Human-readable summary.
            std::string info = typeStr + " ID=" + idHex;
            if (extended) info += "(Ext)";
            info += " DLC=" + std::string{dlcStr};
            if (!hexStr.empty()) { info += " ["; info += hexStr; info += "]"; }
            items.emplace_back("info", std::move(info));

            batch.emplace_back(m_eventId++, std::move(items));
        }

        // Flush batch periodically.
        if (batch.size() >= kProgressBatchSize)
        {
            NotifyNewEventBatch(std::move(batch));
            NotifyProgressUpdated();
            batch.clear();
            batch.reserve(kProgressBatchSize);
        }
    }

    // Flush remaining events.
    if (!batch.empty())
    {
        NotifyNewEventBatch(std::move(batch));
        NotifyProgressUpdated();
    }

    util::Logger::Info("AscParser: parsed {} events from {} lines",
        m_eventId, linesRead);
}

} // namespace parser
