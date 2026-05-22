#include "EvlogTemplateRegistry.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace parser {

// ---------------------------------------------------------------------------
// File-format helpers
// ---------------------------------------------------------------------------

static std::string Trim(const std::string& s)
{
    const auto b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return {};
    const auto e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

static std::string Unquote(const std::string& s)
{
    // Strip surrounding double-quotes if present.
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

static std::string StripComment(const std::string& line)
{
    // Strip // comment
    const auto pos = line.find("//");
    if (pos != std::string::npos) return line.substr(0, pos);
    // Strip # comment
    const auto hash = line.find('#');
    if (hash != std::string::npos) return line.substr(0, hash);
    return line;
}

// Strip optional % delimiters: %facility% → facility
static std::string NormalizeKeyword(const std::string& s)
{
    if (s.size() >= 2 && s.front() == '%' && s.back() == '%')
        return s.substr(1, s.size() - 2);
    return s;
}

// ---------------------------------------------------------------------------
// Facility name → numeric code
// ---------------------------------------------------------------------------

static int32_t ParseFacility(const std::string& s)
{
    // Try numeric first (decimal or 0x hex).
    try {
        size_t pos = 0;
        long v = std::stol(s, &pos, 0);
        if (pos == s.size()) return static_cast<int32_t>(v);
    } catch (...) {}

    static const std::unordered_map<std::string, int32_t> kNames = {
        {"LOG_KERN", 0}, {"LOG_USER", 8}, {"LOG_MAIL", 16}, {"LOG_DAEMON", 24},
        {"LOG_AUTH", 32}, {"LOG_SYSLOG", 40}, {"LOG_LPR", 48}, {"LOG_NEWS", 56},
        {"LOG_UUCP", 64}, {"LOG_CRON", 72}, {"LOG_AUTHPRIV", 80}, {"LOG_FTP", 88},
        {"LOG_LOCAL0", 128}, {"LOG_LOCAL1", 136}, {"LOG_LOCAL2", 144},
        {"LOG_LOCAL3", 152}, {"LOG_LOCAL4", 160}, {"LOG_LOCAL5", 168},
        {"LOG_LOCAL6", 176}, {"LOG_LOCAL7", 184},
    };
    auto it = kNames.find(s);
    return (it != kNames.end()) ? it->second : -1;
}

static uint32_t ParseUint32(const std::string& s)
{
    try {
        size_t pos = 0;
        unsigned long v = std::stoul(s, &pos, 0);
        if (pos == s.size()) return static_cast<uint32_t>(v);
    } catch (...) {}
    return 0;
}

// ---------------------------------------------------------------------------
// Field-declaration parser
// "int uid;"   "char hostname[64];"   "string msg;"
// ---------------------------------------------------------------------------

static EvlogTemplateField::Type MapType(const std::string& t)
{
    if (t == "char"   || t == "int8_t"   || t == "int8")   return EvlogTemplateField::Type::Int8;
    if (t == "uchar"  || t == "uint8_t"  || t == "uint8"
        || t == "byte") return EvlogTemplateField::Type::UInt8;
    if (t == "short"  || t == "int16_t"  || t == "int16")  return EvlogTemplateField::Type::Int16;
    if (t == "ushort" || t == "uint16_t" || t == "uint16") return EvlogTemplateField::Type::UInt16;
    if (t == "int"    || t == "int32_t"  || t == "int32"
        || t == "long") return EvlogTemplateField::Type::Int32;
    if (t == "uint"   || t == "uint32_t" || t == "uint32"
        || t == "ulong") return EvlogTemplateField::Type::UInt32;
    if (t == "longlong" || t == "int64_t"  || t == "int64") return EvlogTemplateField::Type::Int64;
    if (t == "ulonglong"|| t == "uint64_t" || t == "uint64") return EvlogTemplateField::Type::UInt64;
    if (t == "float")   return EvlogTemplateField::Type::Float;
    if (t == "double")  return EvlogTemplateField::Type::Double;
    if (t == "string" || t == "wstring") return EvlogTemplateField::Type::CString;
    return EvlogTemplateField::Type::Int32; // fallback
}

static std::optional<EvlogTemplateField> ParseFieldDecl(std::string line)
{
    // Strip semicolon.
    if (const auto sc = line.rfind(';'); sc != std::string::npos)
        line = line.substr(0, sc);
    line = Trim(line);
    if (line.empty()) return std::nullopt;

    // Tokenise: first token is type, remainder is "name" or "name[N]".
    const auto ws = line.find_first_of(" \t");
    if (ws == std::string::npos) return std::nullopt;

    const std::string typeTok = Trim(line.substr(0, ws));
    std::string rest = Trim(line.substr(ws + 1));

    EvlogTemplateField field;
    field.type = MapType(typeTok);

    // Check for array bracket: name[N]
    const auto bracket = rest.find('[');
    if (bracket != std::string::npos) {
        field.name = Trim(rest.substr(0, bracket));
        const auto closing = rest.find(']', bracket);
        if (closing != std::string::npos)
            field.fixedSize = std::stoul(rest.substr(bracket + 1, closing - bracket - 1));

        // char[N] / string[N] → fixed-size String
        if (field.type == EvlogTemplateField::Type::Int8   ||
            field.type == EvlogTemplateField::Type::UInt8  ||
            field.type == EvlogTemplateField::Type::CString)
        {
            field.type = EvlogTemplateField::Type::String;
        }
    } else {
        field.name = Trim(rest);
    }

    if (field.name.empty()) return std::nullopt;
    return field;
}

// ---------------------------------------------------------------------------
// Core file parser
// ---------------------------------------------------------------------------

static void CommitTemplate(EvlogTemplate& tmpl,
    std::map<std::pair<int32_t,uint32_t>, EvlogTemplate>& out)
{
    if (tmpl.facility < 0 || (tmpl.eventType == 0 && tmpl.fields.empty())) return;
    const auto key = std::make_pair(tmpl.facility, tmpl.eventType);
    out.emplace(key, std::move(tmpl));
    tmpl = {};
}

void EvlogTemplateRegistry::LoadFromFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file) {
        util::Logger::Warn("[EvlogTemplateRegistry] cannot open: {}", path.string());
        return;
    }

    enum class State { Scanning, InTemplate, InAttributes };
    State state = State::Scanning;
    EvlogTemplate current;
    bool hasFacility = false;

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(StripComment(line));
        if (line.empty()) continue;

        // Record separator
        if (line == "---") {
            if (hasFacility) CommitTemplate(current, m_templates);
            hasFacility = false;
            state = State::Scanning;
            continue;
        }

        // Inside attributes block
        if (state == State::InAttributes) {
            if (line == "}") { state = State::InTemplate; continue; }
            if (auto f = ParseFieldDecl(line)) current.fields.push_back(*f);
            continue;
        }

        // Split keyword from value
        const auto ws = line.find_first_of(" \t");
        std::string kw  = NormalizeKeyword(ws == std::string::npos ? line : line.substr(0, ws));
        std::string val = ws == std::string::npos ? "" : Trim(line.substr(ws + 1));

        // Lower-case keyword for comparison
        std::string kwl = kw;
        std::transform(kwl.begin(), kwl.end(), kwl.begin(),
            [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        if (kwl == "facility") {
            if (hasFacility) CommitTemplate(current, m_templates);
            current   = {};
            hasFacility = true;
            state     = State::InTemplate;
            current.facility = ParseFacility(Trim(val));
        } else if (kwl == "event_type" && state == State::InTemplate) {
            current.eventType = ParseUint32(Trim(val));
        } else if (kwl == "description" && state == State::InTemplate) {
            current.description = Unquote(Trim(val));
        } else if (kwl == "format" && state == State::InTemplate) {
            // The format value may span the rest of the line, possibly quoted.
            current.formatStr = Unquote(Trim(val));
        } else if (kwl == "attributes" && state == State::InTemplate) {
            // Opening brace may be on the same line or the next line.
            if (val.find('{') != std::string::npos) {
                // Opening brace already on this line; handle inline content if any.
                const auto open = val.find('{');
                const auto close = val.find('}', open);
                if (close != std::string::npos) {
                    // Inline single-field (unlikely but handle gracefully)
                    std::string inner = Trim(val.substr(open + 1, close - open - 1));
                    if (!inner.empty())
                        if (auto f = ParseFieldDecl(inner)) current.fields.push_back(*f);
                } else {
                    state = State::InAttributes;
                }
            } else {
                // Brace will appear on next non-blank line — peek-ahead not possible
                // with getline, so just enter InAttributes; the '{' line will be a
                // no-op since it contains only punctuation.
                state = State::InAttributes;
            }
        } else if (line == "{" && state == State::InTemplate) {
            // Standalone opening brace after "attributes"
            state = State::InAttributes;
        }
    }

    if (hasFacility) CommitTemplate(current, m_templates);
}

void EvlogTemplateRegistry::LoadFromDirectory(const std::filesystem::path& dir)
{
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();
        if (ext != ".t" && ext != ".tmpl" && ext != ".template") continue;
        LoadFromFile(entry.path());
    }
    if (ec)
        util::Logger::Warn("[EvlogTemplateRegistry] directory iteration error: {}", ec.message());
    util::Logger::Info("[EvlogTemplateRegistry] loaded {} template(s) from {}",
        m_templates.size(), dir.string());
}

// ---------------------------------------------------------------------------

const EvlogTemplate* EvlogTemplateRegistry::Find(int32_t facility,
    uint32_t eventType) const
{
    auto it = m_templates.find({facility, eventType});
    return (it != m_templates.end()) ? &it->second : nullptr;
}

} // namespace parser
