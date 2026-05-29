#pragma once

#include "IDataParser.hpp"
#include "dbc/DbcParser.hpp"
#include "Result.hpp"
#include "Error.hpp"

#include <cstdint>
#include <filesystem>
#include <istream>

namespace parser
{

/**
 * @brief Parses Vector CANalyzer ASC log files (.asc).
 *
 * Supports standard CAN frames (11-bit) and extended CAN frames (29-bit, 'X' suffix).
 * An optional DBC database path may be supplied at construction time; when present,
 * signal values decoded from the DBC are appended as "SIG:<name>" fields.
 *
 * Parsed LogEvent fields:
 *   timestamp   — float seconds from start of trace
 *   type        — "Rx", "Tx", "TxRq", or "ErrorFrame"
 *   CAN_Channel — integer channel number
 *   CAN_ID      — upper-case hex frame ID (e.g. "0B3")
 *   CAN_IDE     — "Standard" or "Extended"
 *   CAN_DLC     — data length code (0-8)
 *   CAN_Data    — space-separated hex bytes (e.g. "F0 00 00 00 00 00 00 00")
 *   CAN_MsgName — message name from DBC (omitted when no DBC or unknown ID)
 *   SIG:<name>  — decoded signal value with unit (omitted when no DBC)
 *   info        — human-readable one-line summary
 */
class AscParser : public IDataParser
{
  public:
    /**
     * @param dbcPath Optional path to a DBC file for signal decoding.
     *                Leave empty (default) to skip DBC decoding.
     */
    explicit AscParser(std::filesystem::path dbcPath = {});
    ~AscParser() override = default;

    void ParseData(const std::filesystem::path& filepath) override;
    void ParseData(std::istream& input) override;

    uint32_t GetCurrentProgress() const override { return m_currentProgress; }
    uint32_t GetTotalProgress()   const override { return m_totalProgress; }

  private:
    std::filesystem::path  m_dbcPath;
    dbc::DbcDatabase       m_dbc;
    bool                   m_dbcLoaded {false};

    uint32_t m_currentProgress {0};
    uint32_t m_totalProgress   {0};
    int      m_eventId         {0};

    /**
     * @brief Loads the DBC database from m_dbcPath.
     *
     * Returns Ok(monostate) if no DBC path is configured or the DBC loaded
     * successfully. Returns Err with FileNotFound/ParseError if the path is
     * set but the file cannot be opened or parsed.
     */
    util::Result<std::monostate, error::Error> LoadDbc();

    void ParseStream(std::istream& input);
};

} // namespace parser
