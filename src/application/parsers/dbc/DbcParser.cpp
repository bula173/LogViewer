#include "DbcParser.hpp"
#include "Error.hpp"
#include "Logger.hpp"
#include "Result.hpp"

#include <charconv>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace parser::dbc
{

namespace
{

using ParseSignalResult = util::Result<DbcSignal, error::Error>;

// Trim leading/trailing whitespace.
std::string_view Trim(std::string_view s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string_view::npos) return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Parse a double from a string_view, returns 0.0 on failure.
double ToDouble(std::string_view sv)
{
    const std::string s{sv};
    try { return std::stod(s); }
    catch (...) { return 0.0; }
}

// Parse a uint32 from a string_view, returns 0 on failure.
uint32_t ToU32(std::string_view sv)
{
    uint32_t v = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return v;
}

// Regex for a signal definition line:
//  SG_ <name> : <startBit>|<length>@<byteOrder><signedness> (<factor>,<offset>) [<min>|<max>] "<unit>" <receivers>
// Groups: 1=name 2=startBit 3=length 4=byteOrder 5=signed 6=factor 7=offset 8=min 9=max 10=unit
const std::regex kSignalRe{
    R"(^\s*SG_\s+(\S+)\s*:\s*)"
    R"((\d+)\|(\d+)@([01])([+-])\s*)"
    R"(\(([^,]+),([^)]+)\)\s*)"
    R"(\[([^|]+)\|([^\]]+)\]\s*)"
    "\"([^\"]*)\""};

ParseSignalResult ParseSignalLine(const std::string& line)
{
    std::smatch m;
    if (!std::regex_search(line, m, kSignalRe))
    {
        const std::string msg = "Cannot parse signal: " + line;
        return ParseSignalResult::Err(error::Error(error::ErrorCode::ParseError, msg, /*showMsgBox=*/false));
    }

    DbcSignal sig;
    sig.name      = m[1].str();
    sig.startBit  = ToU32(m[2].str());
    sig.length    = ToU32(m[3].str());
    sig.isIntel   = (m[4].str() == "1");
    sig.isSigned  = (m[5].str() == "-");
    sig.factor    = ToDouble(m[6].str());
    sig.offset    = ToDouble(m[7].str());
    sig.minVal    = ToDouble(m[8].str());
    sig.maxVal    = ToDouble(m[9].str());
    sig.unit      = m[10].str();
    util::Logger::Trace("DbcParser: parsed signal '{}' startBit={} length={} factor={} offset={}",
        sig.name, sig.startBit, sig.length, sig.factor, sig.offset);
    return ParseSignalResult::Ok(std::move(sig));
}

// BO_ <id> <name>: <dlc> <sender>
bool TryParseMessageHeader(const std::string& line, DbcMessage& msg)
{
    if (line.substr(0, 4) != "BO_ ")
        return false;
    std::istringstream ss(line.substr(4));
    uint32_t id = 0;
    std::string nameColon;
    uint32_t dlc = 0;
    std::string sender;
    if (!(ss >> id >> nameColon >> dlc >> sender))
    {
        util::Logger::Warn("DbcParser: malformed BO_ header: '{}'", line);
        return false;
    }
    msg.id     = id & 0x1FFFFFFFu; // strip extended-frame bit (bit 31 set by some tools)
    msg.name   = nameColon;
    if (!msg.name.empty() && msg.name.back() == ':')
        msg.name.pop_back();
    msg.dlc    = dlc;
    msg.sender = sender;
    util::Logger::Debug("DbcParser: message header id=0x{:X} name='{}' dlc={} sender='{}'",
        msg.id, msg.name, msg.dlc, msg.sender);
    return true;
}

} // namespace

DbcDatabase ParseDbcFile(const std::filesystem::path& path)
{
    DbcDatabase db;

    if (path.empty())
        return db;

    std::ifstream file(path);
    if (!file.is_open())
    {
        util::Logger::Error("DbcParser: cannot open '{}' — file not found or not readable",
            path.string());
        return db;
    }
    util::Logger::Debug("DbcParser: opened '{}'", path.string());

    DbcMessage* current = nullptr;
    std::string line;
    uint32_t signalCount = 0;

    while (std::getline(file, line))
    {
        const std::string_view trimmed = Trim(line);
        if (trimmed.empty())
            continue;

        // Known keyword sections that reset context (VERSION, NS_, BU_, etc.)
        // Log section entry at Debug level so we can follow the parse progression.
        const struct { const char* prefix; size_t len; } kSections[] = {
            {"VERSION", 7}, {"NS_", 3}, {"BU_", 3}, {"VAL_DEF_", 8},
            {"ENVVAR_", 7}, {"SIG_VALTYPE_", 12},
        };
        bool isSectionKeyword = false;
        for (const auto& s : kSections)
        {
            if (trimmed.substr(0, s.len) == s.prefix)
            {
                util::Logger::Debug("DbcParser: section '{}' in '{}'",
                    std::string{trimmed.substr(0, s.len)}, path.filename().string());
                isSectionKeyword = true;
                current = nullptr;
                break;
            }
        }
        if (isSectionKeyword)
            continue;

        if (trimmed.substr(0, 4) == "BO_ ")
        {
            DbcMessage msg;
            if (TryParseMessageHeader(std::string{trimmed}, msg))
            {
                const uint32_t newId = msg.id;
                db.messages[newId] = std::move(msg);
                current = &db.messages[newId];
            }
            continue;
        }

        if (trimmed.substr(0, 4) == "SG_ " && current)
        {
            auto result = ParseSignalLine(std::string{trimmed});
            if (result.isOk())
            {
                current->signalDefs.push_back(result.unwrap());
                ++signalCount;
            }
            else
            {
                util::Logger::Warn("DbcParser: skip malformed signal: {}", result.error().what());
            }
            continue;
        }

        if (trimmed.substr(0, 4) == "SG_ " && !current)
        {
            util::Logger::Warn("DbcParser: SG_ line outside BO_ block — skipped: '{}'",
                std::string{trimmed.substr(0, std::min(trimmed.size(), size_t{60}))});
            continue;
        }

        // Any non-indented line outside a BO_ block resets the current message pointer.
        if (trimmed[0] != ' ' && trimmed[0] != '\t')
        {
            // Log truly unknown/unexpected top-level tokens as Warn.
            const struct { const char* prefix; size_t len; } kKnown[] = {
                {"BO_TX_BU_", 9}, {"EV_", 3}, {"CM_", 3}, {"BA_DEF_", 7},
                {"BA_", 3}, {"VAL_", 4}, {"SIG_GROUP_", 10}, {"FILTER", 6},
            };
            bool known = false;
            for (const auto& k : kKnown)
                if (trimmed.substr(0, k.len) == k.prefix) { known = true; break; }
            if (!known)
                util::Logger::Warn("DbcParser: unknown token '{}' — ignored",
                    std::string{trimmed.substr(0, std::min(trimmed.size(), size_t{40}))});
            current = nullptr;
        }
    }

    util::Logger::Info("DbcParser: loaded {} message(s), {} signal(s) from '{}'",
        db.messages.size(), signalCount, path.string());
    return db;
}

} // namespace parser::dbc
