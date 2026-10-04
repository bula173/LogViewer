#pragma once

#include "IDataParser.hpp"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace parser
{

/**
 * @brief Parses merged test-run logs produced by the safeAPI RBC 2oo2 test
 *        environment (robot steps + container + simulator logs in one file).
 *
 * File layout: a `#`-prefixed header block, then one event per line, each field
 * wrapped in square brackets:
 *
 * @code
 * [timestamp] [category] [source] [destination] [level] [event type] [info] [payload]
 * @endcode
 *
 * Brackets may nest inside `info` and `payload` (e.g. `[[ 419310003] [GW-IL EAST] starting]`),
 * so fields are split by bracket depth, not by a fixed regex. `payload` is
 * everything after the 7th field; it may hold several bracket groups
 * (`[train_id=1] [P0] [cycle=1 ...]`) — those are kept verbatim.
 *
 * Parsed LogEvent fields:
 *   timestamp   — ISO-8601 UTC string, unchanged (e.g. "2026-09-19T06:46:46.456779Z")
 *   category    — "GENERAL" or "IO"
 *   source      — emitting component (e.g. "a-west", "robot", "il-multi")
 *   destination — target component(s) or "internal"
 *   level       — INFO / DEBUG / ERROR / FAIL
 *   event_type  — LOG, STEP, ENVELOPE, CMD_RESP, ...
 *   info        — short description
 *   payload     — trailing data (omitted when empty)
 *   unit        — 2oo2 channel from an `info` prefix like `[a-west | GP] …` (optional)
 *   p.<key>     — structured payload content, see ExtractPayloadFields() (optional)
 *
 * The header's `# Key : Value` lines are not events; they are available
 * through GetFileMetadata() after parsing.
 *
 * Lines that do not match the layout are skipped. If no line matches at all,
 * ParseData() throws error::Error(ParseError) so a wrong file-type choice is
 * reported instead of silently producing an empty view.
 */
class SapiLogParser : public IDataParser
{
  public:
    SapiLogParser() = default;
    ~SapiLogParser() override = default;

    void ParseData(const std::filesystem::path& filepath) override;
    void ParseData(std::istream& input) override;

    uint32_t GetCurrentProgress() const override { return m_currentProgress; }
    uint32_t GetTotalProgress()   const override { return m_totalProgress; }

    /// The `# Key : Value` lines of the header block (before the first event),
    /// in file order, e.g. {"Test Case", "IL Grants A Movement Authority"},
    /// {"Status", "PASS"}, {"Timestamp", …}, {"Sources", …}, {"Entries", "1129"}.
    std::vector<std::pair<std::string, std::string>> GetFileMetadata() const override
    {
        return m_metadata;
    }

    /// True when the first lines of @p filepath carry the merged-log header
    /// (`# FULL MERGED TEST STEPS, CONTAINER & SIMULATOR LOGS`). Used to
    /// auto-select this parser for generic `.txt` files.
    static bool LooksLikeSapiLog(const std::filesystem::path& filepath);

    /// Parses one line into @p out. Returns false for header, blank and
    /// malformed lines. Exposed for tests.
    static bool ParseLine(std::string_view line, db::LogEvent::EventItems& out);

    /// Appends a header line such as `# Test Case   : IL Grants …` (starting
    /// with '#') to @p metadata as {"Test Case", "IL Grants …"}; other comment
    /// lines (rulers, the title line) are ignored.
    static void ParseHeaderLine(std::string_view line,
                                std::vector<std::pair<std::string, std::string>>& metadata);

    /// Returns the unit of an `info` field that starts with a `[unit | partition]`
    /// marker (e.g. "a-west" for `[a-west | GP] Log message`), else an empty view.
    static std::string_view ExtractUnit(std::string_view info);

    /// Appends the structured content of @p payload to @p out as `p.<key>` fields.
    /// The raw payload is never modified. Recognised shapes:
    ///  - a JSON object: top-level values become `p.<key>`, members of a nested
    ///    object `p.<key>.<member>`; strings are unquoted, anything deeper is
    ///    kept as compact JSON text;
    ///  - whitespace-separated `key=value` tokens (values may be quoted or hold
    ///    balanced `[...]`/`{...}`); bracket groups of such tokens are unwrapped
    ///    and single-word label groups (`[P0]`) are ignored.
    /// Anything else — free text, a single non-`key=value` token, invalid JSON,
    /// unbalanced brackets — yields no fields. Bounded per event: payloads over
    /// 4 KiB are skipped, at most 32 fields, over-long keys/values and repeated
    /// keys are dropped (first occurrence wins).
    static void ExtractPayloadFields(std::string_view payload, db::LogEvent::EventItems& out);

  private:
    void ParseStream(std::istream& input);

    uint32_t m_currentProgress {0};
    uint32_t m_totalProgress   {0};
    std::vector<std::pair<std::string, std::string>> m_metadata;
};

} // namespace parser
