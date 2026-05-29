#pragma once

#include "Error.hpp"
#include "IDataParser.hpp"
#include "Result.hpp"

#include <filesystem>
#include <iosfwd>

namespace parser {

/// Parser for AUTOSAR Diagnostic Log and Trace (DLT) binary files (.dlt).
///
/// Handles files with and without the 16-byte storage header (DLT\x01 magic).
/// Decodes verbose message payloads (string, integer, float arguments) into
/// human-readable text; non-verbose messages are shown as MsgID + hex dump.
///
/// Emitted event fields:
///   timestamp  — seconds since trace start (from WTMS header or storage header)
///   level      — Fatal | Error | Warn | Info | Debug | Verbose  (log messages)
///   type       — Log | AppTrace | NwTrace | Control
///   AppID      — 4-char application identifier
///   ContextID  — 4-char context identifier
///   EcuID      — 4-char ECU identifier
///   MsgCtr     — 8-bit message counter (wraps at 255)
///   info       — decoded payload text
class DltParser : public IDataParser
{
  public:
    void ParseData(const std::filesystem::path& filepath) override;
    void ParseData(std::istream& input) override;

    uint32_t GetCurrentProgress() const override { return m_currentProgress; }
    uint32_t GetTotalProgress()   const override { return m_totalProgress; }

  private:
    /// Parse all messages from @p input.
    /// Returns the number of messages emitted on success, or an error
    /// if the stream could not be probed (e.g. immediate read failure).
    util::Result<int, error::Error> ParseStream(std::istream& input);

    uint32_t m_currentProgress {0};
    uint32_t m_totalProgress   {0};
    int      m_eventId         {0};
};

} // namespace parser
