#pragma once

#include "Error.hpp"
#include "EvlogTemplateRegistry.hpp"
#include "IDataParser.hpp"
#include "Result.hpp"

#include <filesystem>
#include <iosfwd>
#include <memory>

namespace parser {

/// Parser for Linux Enterprise Event Logging (evlog) binary files.
///
/// Implements a self-contained reader for the POSIX 1003.25 evlog binary
/// format as produced by the evlogd daemon (evlog.sourceforge.net).
///
/// ## File format
/// A flat stream of variable-length records with no file-level header.
/// Each record consists of:
///   - A 60-byte fixed header (posix_log_entry, 32-bit little-endian layout)
///   - log_size bytes of variable payload (may be 0)
///
/// ## Fixed header layout (all little-endian)
///   Offset  Size  Field
///    0       4    log_recid       — record serial number
///    4       4    log_size        — bytes of payload following this header
///    8       4    log_format      — payload encoding (see Format enum)
///   12       4    log_event_type  — application-defined event type
///   16       4    log_facility    — syslog facility code (LOG_USER=8, …)
///   20       4    log_severity    — syslog severity (0=EMERG … 7=DEBUG)
///   24       4    log_uid         — real UID of logging process
///   28       4    log_gid         — real GID of logging process
///   32       4    log_pid         — PID
///   36       4    log_pgrp        — process group ID
///   40       4    log_time.tv_sec — UNIX timestamp (seconds)
///   44       4    log_time.tv_nsec— sub-second nanoseconds
///   48       4    log_flags       — EVL_* flag bits
///   52       4    log_thread      — pthread_t (32-bit value)
///   56       4    log_processor   — CPU number
///
/// ## Payload encoding (log_format values)
///   0  NODATA  — no payload (log_size should be 0)
///   1  BINARY  — raw binary blob; emitted as hex dump
///   2  STRING  — null-terminated UTF-8/ASCII string; emitted as info
///   3  PRINTF  — null-terminated format string + binary varargs;
///               format string is extracted as info, varargs as hex
///
/// ## Emitted event fields
///   timestamp   — float seconds (tv_sec.tv_nsec)
///   level       — Emergency|Alert|Critical|Error|Warning|Notice|Info|Debug
///   facility    — kern|user|mail|daemon|auth|syslog|lpr|news|uucp|
///                 cron|authpriv|ftp|local0…local7|unknown
///   event_type  — hex string (e.g. "0x00000001")
///   pid         — decimal PID
///   uid         — decimal UID
///   flags       — comma-separated EVL flag names (omitted if none set)
///   recid       — decimal record ID
///   cpu         — decimal CPU number (omitted if 0)
///   info        — decoded payload text
class EvlogParser : public IDataParser
{
  public:
    /// Payload encoding constants (log_format field).
    enum Format : int32_t {
        NODATA = 0,
        BINARY = 1,
        STRING = 2,
        PRINTF = 3,
    };

    void ParseData(const std::filesystem::path& filepath) override;
    void ParseData(std::istream& input) override;

    uint32_t GetCurrentProgress() const override { return m_currentProgress; }
    uint32_t GetTotalProgress()   const override { return m_totalProgress; }

    /// Load all *.t/*.tmpl/*.template files from @p dir and use them to decode
    /// BINARY payloads. Must be called before ParseData().
    /// Returns Err if the directory cannot be iterated (e.g. does not exist).
    util::Result<void, error::Error> SetTemplateDirectory(const std::filesystem::path& dir);

    /// Load a single template file. May be called multiple times.
    void SetTemplateFile(const std::filesystem::path& path);

  private:
    /// Parse all records from @p input.
    /// Returns the number of records emitted on success, or an error
    /// if the stream yields zero bytes on the very first header read.
    util::Result<int, error::Error> ParseStream(std::istream& input);

    uint32_t m_currentProgress {0};
    uint32_t m_totalProgress   {0};
    int      m_eventId         {0};
    std::unique_ptr<EvlogTemplateRegistry> m_registry;
};

} // namespace parser
