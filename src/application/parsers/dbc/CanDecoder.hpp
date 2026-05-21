#pragma once

#include "DbcParser.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace parser::dbc
{

// Decode all signals from a CAN frame and return them as key-value pairs.
// canId    — raw CAN frame ID (no extended-frame bit)
// data     — raw payload bytes (up to 8)
// db       — loaded DBC database
// Returns vector of ("SIG:<signal_name>", "<physical_value> <unit>") pairs.
// Returns empty vector if canId is not in the database.
std::vector<std::pair<std::string, std::string>> DecodeFrame(
    uint32_t canId,
    const std::vector<uint8_t>& data,
    const DbcDatabase& db);

// Extract the raw integer value for a single signal from the payload.
// Exposed for testing.
int64_t ExtractRawValue(const DbcSignal& sig, const std::vector<uint8_t>& data);

} // namespace parser::dbc
