#include "DbcParser.hpp"
#include "Logger.hpp"

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

DbcSignal ParseSignalLine(const std::string& line)
{
    std::smatch m;
    if (!std::regex_search(line, m, kSignalRe))
        throw std::runtime_error("Cannot parse signal: " + line);

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
    return sig;
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
        return false;
    msg.id     = id & 0x1FFFFFFFu; // strip extended-frame bit (bit 31 set by some tools)
    msg.name   = nameColon;
    if (!msg.name.empty() && msg.name.back() == ':')
        msg.name.pop_back();
    msg.dlc    = dlc;
    msg.sender = sender;
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
        util::Logger::Warn("DbcParser: cannot open '{}'", path.string());
        return db;
    }

    DbcMessage* current = nullptr;
    std::string line;

    while (std::getline(file, line))
    {
        const std::string_view trimmed = Trim(line);

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
            try
            {
                current->signalDefs.push_back(ParseSignalLine(std::string{trimmed}));
            }
            catch (const std::exception& ex)
            {
                util::Logger::Warn("DbcParser: skip malformed signal: {}", ex.what());
            }
            continue;
        }

        // Any non-indented line outside a BO_ block resets the current message pointer.
        if (!trimmed.empty() && trimmed[0] != ' ' && trimmed[0] != '\t' && trimmed.substr(0, 4) != "SG_ ")
            current = nullptr;
    }

    util::Logger::Info("DbcParser: loaded {} messages from '{}'",
        db.messages.size(), path.string());
    return db;
}

} // namespace parser::dbc
