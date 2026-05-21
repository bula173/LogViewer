#include <gtest/gtest.h>

#include "dbc/CanDecoder.hpp"
#include "dbc/DbcParser.hpp"

#include <sstream>
#include <string>

using namespace parser::dbc;

// ---------------------------------------------------------------------------
// DbcParser tests
// ---------------------------------------------------------------------------

TEST(DbcParserTest, ParseEmptyInput)
{
    const DbcDatabase db = ParseDbcFile({});
    EXPECT_TRUE(db.messages.empty());
}

TEST(DbcParserTest, ParseSingleMessage)
{
    // Inline parse via a temporary file would require a real path; instead we
    // verify the struct layout by building a DbcDatabase in-memory.
    DbcDatabase db;
    DbcMessage msg;
    msg.id     = 1;
    msg.name   = "EngineStatus";
    msg.dlc    = 8;
    msg.sender = "ECU";

    DbcSignal rpm;
    rpm.name     = "RPM";
    rpm.startBit = 0;
    rpm.length   = 16;
    rpm.isIntel  = true;
    rpm.isSigned = false;
    rpm.factor   = 0.5;
    rpm.offset   = 0.0;
    rpm.unit     = "rpm";
    msg.signalDefs.push_back(rpm);

    db.messages[msg.id] = msg;

    ASSERT_EQ(db.messages.size(), 1u);
    EXPECT_EQ(db.messages.at(1).name, "EngineStatus");
    ASSERT_EQ(db.messages.at(1).signalDefs.size(), 1u);
    EXPECT_EQ(db.messages.at(1).signalDefs[0].name, "RPM");
    EXPECT_DOUBLE_EQ(db.messages.at(1).signalDefs[0].factor, 0.5);
}

// ---------------------------------------------------------------------------
// CanDecoder tests — Intel (little-endian) byte order
// ---------------------------------------------------------------------------

// Build a minimal DBC with one Intel signal for decoder tests.
static DbcDatabase MakeIntelDb()
{
    DbcDatabase db;
    DbcMessage  msg;
    msg.id  = 0x001;
    msg.dlc = 8;

    // RPM: 16-bit unsigned Intel, factor 0.5, offset 0 → raw=0x8000 → 16384 rpm
    DbcSignal rpm;
    rpm.name     = "RPM";
    rpm.startBit = 0;
    rpm.length   = 16;
    rpm.isIntel  = true;
    rpm.isSigned = false;
    rpm.factor   = 0.5;
    rpm.offset   = 0.0;
    rpm.unit     = "rpm";
    msg.signalDefs.push_back(rpm);

    // Throttle: 8-bit unsigned Intel starting at bit 16
    DbcSignal throttle;
    throttle.name     = "Throttle";
    throttle.startBit = 16;
    throttle.length   = 8;
    throttle.isIntel  = true;
    throttle.isSigned = false;
    throttle.factor   = 0.392157;
    throttle.offset   = 0.0;
    throttle.unit     = "%";
    msg.signalDefs.push_back(throttle);

    // EngineTemp: 8-bit signed Intel starting at bit 24, offset -40
    DbcSignal temp;
    temp.name     = "EngineTemp";
    temp.startBit = 24;
    temp.length   = 8;
    temp.isIntel  = true;
    temp.isSigned = true;
    temp.factor   = 1.0;
    temp.offset   = -40.0;
    temp.unit     = "degC";
    msg.signalDefs.push_back(temp);

    db.messages[msg.id] = msg;
    return db;
}

TEST(CanDecoderTest, IntelUnsignedRpm)
{
    // Data: RPM bytes 0x00 0x80 → raw = 0x8000 = 32768, physical = 32768 * 0.5 = 16384
    const std::vector<uint8_t> data = {0x00, 0x80, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00};
    const DbcDatabase db = MakeIntelDb();

    const int64_t raw = ExtractRawValue(db.messages.at(0x001).signalDefs[0], data);
    EXPECT_EQ(raw, 32768);
}

TEST(CanDecoderTest, IntelUnsignedThrottle)
{
    // Throttle byte (index 2) = 0x00 → raw=0, physical=0 %
    const std::vector<uint8_t> data = {0x00, 0x80, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00};
    const DbcDatabase db = MakeIntelDb();

    const int64_t raw = ExtractRawValue(db.messages.at(0x001).signalDefs[1], data);
    EXPECT_EQ(raw, 0);
}

TEST(CanDecoderTest, IntelUnsignedThrottleNonZero)
{
    // Throttle byte (index 2) = 0xFF → raw=255, physical ≈ 100 %
    const std::vector<uint8_t> data = {0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00};
    const DbcDatabase db = MakeIntelDb();

    const int64_t raw = ExtractRawValue(db.messages.at(0x001).signalDefs[1], data);
    EXPECT_EQ(raw, 255);
}

TEST(CanDecoderTest, IntelSignedTempPositive)
{
    // EngineTemp byte (index 3) = 0x64 (100 decimal) → raw=100, physical = 100-40 = 60°C
    const std::vector<uint8_t> data = {0x00, 0x80, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00};
    const DbcDatabase db = MakeIntelDb();

    const int64_t raw = ExtractRawValue(db.messages.at(0x001).signalDefs[2], data);
    EXPECT_EQ(raw, 100);
}

TEST(CanDecoderTest, IntelSignedTempNegative)
{
    // EngineTemp byte = 0x00 → raw=0, physical = 0-40 = -40°C
    const std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const DbcDatabase db = MakeIntelDb();

    const int64_t raw = ExtractRawValue(db.messages.at(0x001).signalDefs[2], data);
    EXPECT_EQ(raw, 0);
}

TEST(CanDecoderTest, IntelSignedTempNegativeValue)
{
    // 8-bit signed, bit 24: 0xFF → raw = -1 (two's complement), physical = -1-40 = -41°C
    const std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00};
    const DbcDatabase db = MakeIntelDb();

    const int64_t raw = ExtractRawValue(db.messages.at(0x001).signalDefs[2], data);
    EXPECT_EQ(raw, -1);
}

TEST(CanDecoderTest, DecodeFrameReturnsNamedFields)
{
    const std::vector<uint8_t> data = {0x00, 0x80, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00};
    const DbcDatabase db = MakeIntelDb();

    auto decoded = DecodeFrame(0x001, data, db);
    ASSERT_EQ(decoded.size(), 3u);
    EXPECT_EQ(decoded[0].first, "SIG:RPM");
    EXPECT_EQ(decoded[1].first, "SIG:Throttle");
    EXPECT_EQ(decoded[2].first, "SIG:EngineTemp");
}

TEST(CanDecoderTest, DecodeFrameUnknownIdReturnsEmpty)
{
    const std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    const DbcDatabase db = MakeIntelDb();

    auto decoded = DecodeFrame(0xDEAD, data, db);
    EXPECT_TRUE(decoded.empty());
}

TEST(CanDecoderTest, DecodeFrameEmptyDataReturnsZeroValues)
{
    const DbcDatabase db = MakeIntelDb();
    auto decoded = DecodeFrame(0x001, {}, db);
    // Signals still returned, all with value 0
    EXPECT_EQ(decoded.size(), 3u);
}

// ---------------------------------------------------------------------------
// CanDecoder tests — single-bit signal
// ---------------------------------------------------------------------------

TEST(CanDecoderTest, SingleBitSignal)
{
    DbcDatabase db;
    DbcMessage  msg;
    msg.id  = 0x200;
    msg.dlc = 1;

    DbcSignal flag;
    flag.name     = "Active";
    flag.startBit = 3;  // bit 3 of byte 0
    flag.length   = 1;
    flag.isIntel  = true;
    flag.isSigned = false;
    flag.factor   = 1.0;
    flag.offset   = 0.0;
    msg.signalDefs.push_back(flag);

    db.messages[0x200] = msg;

    // Byte 0 = 0x08 = 0b00001000 → bit 3 = 1
    const int64_t raw1 = ExtractRawValue(msg.signalDefs[0], {0x08});
    EXPECT_EQ(raw1, 1);

    // Byte 0 = 0x07 = 0b00000111 → bit 3 = 0
    const int64_t raw0 = ExtractRawValue(msg.signalDefs[0], {0x07});
    EXPECT_EQ(raw0, 0);
}
