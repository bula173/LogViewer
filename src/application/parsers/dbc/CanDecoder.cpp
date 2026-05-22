#include "CanDecoder.hpp"

#include <cstdint>
#include <format>
#include <string>

namespace parser::dbc
{

namespace
{

// Intel (little-endian): startBit is the LSB position in the flat bit array
// (bit 0 = byte 0 LSB, bit 7 = byte 0 MSB, bit 8 = byte 1 LSB, ...).
int64_t ExtractIntel(const std::vector<uint8_t>& data, uint32_t startBit, uint32_t length,
    bool isSigned)
{
    uint64_t raw = 0;
    for (uint32_t i = 0; i < length; ++i)
    {
        const uint32_t bitPos  = startBit + i;
        const uint32_t byteIdx = bitPos / 8u;
        const uint32_t bitIdx  = bitPos % 8u;
        if (byteIdx >= data.size())
            break;
        if ((data[byteIdx] >> bitIdx) & 0x01u)
            raw |= (uint64_t{1} << i);
    }

    if (isSigned && length > 0 && (raw & (uint64_t{1} << (length - 1))))
    {
        // Sign-extend
        raw |= ~((uint64_t{1} << length) - 1);
        return static_cast<int64_t>(raw);
    }
    return static_cast<int64_t>(raw);
}

// Motorola (big-endian): startBit is the MSB in DBC notation.
// DBC Motorola bit numbering: bit N in byte B is at flat position B*8 + (7-N%8)
// converted to the "network" position used in DBC = byte*8 + (7 - bit_in_byte).
int64_t ExtractMotorola(const std::vector<uint8_t>& data, uint32_t startBit, uint32_t length,
    bool isSigned)
{
    // Convert DBC Motorola startBit (MSB position) to flat bit index:
    // flat = (startBit / 8) * 8 + (7 - startBit % 8)
    uint64_t raw = 0;
    const uint32_t msbFlat = (startBit / 8u) * 8u + (7u - startBit % 8u);

    for (uint32_t i = 0; i < length; ++i)
    {
        // Bits go from MSB downward in "network" order.
        const uint32_t bitFlat = msbFlat - i;  // linear descent, wraps within bytes
        const uint32_t byteIdx = bitFlat / 8u;
        const uint32_t bitIdx  = 7u - (bitFlat % 8u);  // back to data-byte bit index
        if (byteIdx >= data.size())
            break;
        if ((data[byteIdx] >> bitIdx) & 0x01u)
            raw |= (uint64_t{1} << (length - 1u - i));
    }

    if (isSigned && length > 0 && (raw & (uint64_t{1} << (length - 1))))
    {
        raw |= ~((uint64_t{1} << length) - 1);
        return static_cast<int64_t>(raw);
    }
    return static_cast<int64_t>(raw);
}

// Format a physical value: drop trailing zeros from decimals.
std::string FormatPhysical(double v)
{
    // Use up to 6 significant digits; strip unnecessary trailing zeros.
    std::string s = std::format("{:.6g}", v);
    return s;
}

} // namespace

int64_t ExtractRawValue(const DbcSignal& sig, const std::vector<uint8_t>& data)
{
    if (data.empty() || sig.length == 0)
        return 0;
    if (sig.isIntel)
        return ExtractIntel(data, sig.startBit, sig.length, sig.isSigned);
    return ExtractMotorola(data, sig.startBit, sig.length, sig.isSigned);
}

std::vector<std::pair<std::string, std::string>> DecodeFrame(
    uint32_t canId,
    const std::vector<uint8_t>& data,
    const DbcDatabase& db)
{
    std::vector<std::pair<std::string, std::string>> results;

    const auto it = db.messages.find(canId);
    if (it == db.messages.end())
        return results;

    const DbcMessage& msg = it->second;
    results.reserve(msg.signalDefs.size());

    for (const DbcSignal& sig : msg.signalDefs)
    {
        const int64_t raw = ExtractRawValue(sig, data);
        const double physical = static_cast<double>(raw) * sig.factor + sig.offset;
        std::string valStr = FormatPhysical(physical);
        if (!sig.unit.empty())
        {
            valStr += ' ';
            valStr += sig.unit;
        }
        results.emplace_back("SIG:" + sig.name, std::move(valStr));
    }

    return results;
}

} // namespace parser::dbc
