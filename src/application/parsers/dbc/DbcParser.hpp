#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace parser::dbc
{

struct DbcSignal
{
    std::string name;
    uint32_t    startBit {0};
    uint32_t    length   {1};
    bool        isIntel  {true};   // @1 = Intel/little-endian, @0 = Motorola/big-endian
    bool        isSigned {false};
    double      factor   {1.0};
    double      offset   {0.0};
    double      minVal   {0.0};
    double      maxVal   {0.0};
    std::string unit;
};

struct DbcMessage
{
    uint32_t               id     {0};
    std::string            name;
    uint32_t               dlc    {0};
    std::string            sender;
    std::vector<DbcSignal> signalDefs;
};

struct DbcDatabase
{
    // Keyed by raw CAN ID (extended-frame bit not set here; caller strips it)
    std::map<uint32_t, DbcMessage> messages;
};

// Returns a populated DbcDatabase, or an empty one on error/missing file.
// If `ok` is non-null, it is set to false when the file could not be opened
// (distinguishing that from a validly-opened file that simply defines no
// messages, which also returns an empty DbcDatabase but with `ok` left true).
DbcDatabase ParseDbcFile(const std::filesystem::path& path, bool* ok = nullptr);

} // namespace parser::dbc
