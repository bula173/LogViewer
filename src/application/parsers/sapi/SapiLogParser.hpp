#pragma once

#include "IDataParser.hpp"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <string_view>

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

    /// True when the first lines of @p filepath carry the merged-log header
    /// (`# FULL MERGED TEST STEPS, CONTAINER & SIMULATOR LOGS`). Used to
    /// auto-select this parser for generic `.txt` files.
    static bool LooksLikeSapiLog(const std::filesystem::path& filepath);

    /// Parses one line into @p out. Returns false for header, blank and
    /// malformed lines. Exposed for tests.
    static bool ParseLine(std::string_view line, db::LogEvent::EventItems& out);

  private:
    void ParseStream(std::istream& input);

    uint32_t m_currentProgress {0};
    uint32_t m_totalProgress   {0};
};

} // namespace parser
