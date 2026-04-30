/**
 * @file BuiltinConversionPluginsTest.cpp
 * @brief Specification-based unit tests for built-in IFieldConversionPlugin implementations.
 *
 * Each plugin is tested against the behaviour documented in its Doxygen
 * comments, not against the implementation.
 *
 * Plugins under test:
 *  HexToAsciiPlugin  — "hex_to_ascii" : converts even-length all-hex strings to ASCII
 *  UnixToDatePlugin  — "unix_to_date" : converts Unix epoch seconds to "YYYY-MM-DD HH:MM:SS UTC"
 *  ValueMapPlugin    — "value_map"    : looks up value in the parameters map
 *  IsoLatinPlugin    — "iso_latin"    : converts ISO Latin-1 bytes to UTF-8
 *  NidLrbgPlugin     — "nid_lrbg"    : extracts NID_C and NID_BG from 24-bit integer
 *  NidEnginePlugin   — "nid_engine"  : extracts M_VOLTAGE and NID_ENGINE from 24-bit integer
 *  TrackConditionPlugin — "m_trackcond" : decodes 0–11 to ERTMS condition text
 *  TCyclocPlugin     — "t_cycloc"    : converts T_CYCLOC cycle-time integer to human string
 */

#include <gtest/gtest.h>
#include "BuiltinConversionPlugins.hpp"

#include <map>
#include <string>

using Params = std::map<std::string, std::string>;

// ============================================================================
// HexToAsciiPlugin
// ============================================================================

/**
 * @brief Spec: GetConversionType() returns "hex_to_ascii".
 */
TEST(HexToAsciiPluginTest, GetConversionType)
{
    config::HexToAsciiPlugin p;
    EXPECT_EQ(p.GetConversionType(), "hex_to_ascii");
}

/**
 * @brief Spec: converts an even-length all-hex string to its ASCII equivalent.
 * "48656C6C6F" → "Hello"
 */
TEST(HexToAsciiPluginTest, ConvertsHexToAscii)
{
    config::HexToAsciiPlugin p;
    EXPECT_EQ(p.Convert("48656C6C6F", {}), "Hello");
}

/**
 * @brief Spec: lowercase hex digits are also accepted.
 * "48656c6c6f" → "Hello"
 */
TEST(HexToAsciiPluginTest, AcceptsLowercaseHex)
{
    config::HexToAsciiPlugin p;
    EXPECT_EQ(p.Convert("48656c6c6f", {}), "Hello");
}

/**
 * @brief Spec: an odd-length string is returned unchanged (not valid hex pairs).
 */
TEST(HexToAsciiPluginTest, OddLengthReturnedUnchanged)
{
    config::HexToAsciiPlugin p;
    EXPECT_EQ(p.Convert("ABC", {}), "ABC");
}

/**
 * @brief Spec: an empty string is returned unchanged.
 */
TEST(HexToAsciiPluginTest, EmptyStringReturnedUnchanged)
{
    config::HexToAsciiPlugin p;
    EXPECT_EQ(p.Convert("", {}), "");
}

/**
 * @brief Spec: a string containing non-hex characters is returned unchanged.
 */
TEST(HexToAsciiPluginTest, NonHexReturnedUnchanged)
{
    config::HexToAsciiPlugin p;
    EXPECT_EQ(p.Convert("ZZZZ", {}), "ZZZZ");
}

/**
 * @brief Spec: single null byte "00" converts to a string containing '\0'.
 */
TEST(HexToAsciiPluginTest, NullByteConverts)
{
    config::HexToAsciiPlugin p;
    std::string result = p.Convert("00", {});
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], '\0');
}

// ============================================================================
// UnixToDatePlugin
// ============================================================================

/**
 * @brief Spec: GetConversionType() returns "unix_to_date".
 */
TEST(UnixToDatePluginTest, GetConversionType)
{
    config::UnixToDatePlugin p;
    EXPECT_EQ(p.GetConversionType(), "unix_to_date");
}

/**
 * @brief Spec: converts epoch 0 to the Unix epoch date "1970-01-01 00:00:00 UTC".
 */
TEST(UnixToDatePluginTest, ConvertsEpochZero)
{
    config::UnixToDatePlugin p;
    EXPECT_EQ(p.Convert("0", {}), "1970-01-01 00:00:00 UTC");
}

/**
 * @brief Spec: converts a known timestamp to the correct UTC date-time string.
 * 1609459200 = 2021-01-01 00:00:00 UTC
 */
TEST(UnixToDatePluginTest, ConvertsKnownTimestamp)
{
    config::UnixToDatePlugin p;
    EXPECT_EQ(p.Convert("1609459200", {}), "2021-01-01 00:00:00 UTC");
}

/**
 * @brief Spec: a non-numeric string is returned unchanged (no crash).
 */
TEST(UnixToDatePluginTest, NonNumericReturnedUnchanged)
{
    config::UnixToDatePlugin p;
    EXPECT_EQ(p.Convert("not-a-number", {}), "not-a-number");
}

/**
 * @brief Spec: the output always ends with " UTC".
 */
TEST(UnixToDatePluginTest, OutputEndsWithUTC)
{
    config::UnixToDatePlugin p;
    std::string result = p.Convert("1000000", {});
    EXPECT_TRUE(result.rfind(" UTC") == result.size() - 4);
}

// ============================================================================
// ValueMapPlugin
// ============================================================================

/**
 * @brief Spec: GetConversionType() returns "value_map".
 */
TEST(ValueMapPluginTest, GetConversionType)
{
    config::ValueMapPlugin p;
    EXPECT_EQ(p.GetConversionType(), "value_map");
}

/**
 * @brief Spec: when the value is a key in the parameters map, the mapped
 * value is returned.
 */
TEST(ValueMapPluginTest, ReturnsMapValue)
{
    config::ValueMapPlugin p;
    Params params = {{"0", "Off"}, {"1", "On"}};
    EXPECT_EQ(p.Convert("1", params), "On");
    EXPECT_EQ(p.Convert("0", params), "Off");
}

/**
 * @brief Spec: when the value is not found in the map, the original value
 * is returned unchanged.
 */
TEST(ValueMapPluginTest, UnknownValueReturnedUnchanged)
{
    config::ValueMapPlugin p;
    Params params = {{"0", "Off"}};
    EXPECT_EQ(p.Convert("99", params), "99");
}

/**
 * @brief Spec: an empty parameters map returns the original value unchanged.
 */
TEST(ValueMapPluginTest, EmptyParamsReturnOriginal)
{
    config::ValueMapPlugin p;
    EXPECT_EQ(p.Convert("hello", {}), "hello");
}

// ============================================================================
// IsoLatinPlugin
// ============================================================================

/**
 * @brief Spec: GetConversionType() returns "iso_latin".
 */
TEST(IsoLatinPluginTest, GetConversionType)
{
    config::IsoLatinPlugin p;
    EXPECT_EQ(p.GetConversionType(), "iso_latin");
}

/**
 * @brief Spec: pure ASCII characters (< 128) pass through unchanged.
 */
TEST(IsoLatinPluginTest, AsciiCharsPassThrough)
{
    config::IsoLatinPlugin p;
    EXPECT_EQ(p.Convert("Hello", {}), "Hello");
}

/**
 * @brief Spec: Latin-1 bytes ≥ 128 are encoded as two-byte UTF-8 sequences.
 * Each high byte should be split into a two-byte sequence where the first byte
 * has the high bits 0xC0 | (byte >> 6) and the second 0x80 | (byte & 0x3F).
 */
TEST(IsoLatinPluginTest, HighBytesEncodedAsUtf8)
{
    config::IsoLatinPlugin p;
    // 0xE9 = 'é' in Latin-1 → 0xC3 0xA9 in UTF-8
    std::string input;
    input += static_cast<char>(0xE9);
    std::string result = p.Convert(input, {});
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(static_cast<unsigned char>(result[0]), 0xC3u);
    EXPECT_EQ(static_cast<unsigned char>(result[1]), 0xA9u);
}

/**
 * @brief Spec: an empty string produces an empty result.
 */
TEST(IsoLatinPluginTest, EmptyStringProducesEmpty)
{
    config::IsoLatinPlugin p;
    EXPECT_EQ(p.Convert("", {}), "");
}

// ============================================================================
// NidLrbgPlugin
// ============================================================================

/**
 * @brief Spec: GetConversionType() returns "nid_lrbg".
 */
TEST(NidLrbgPluginTest, GetConversionType)
{
    config::NidLrbgPlugin p;
    EXPECT_EQ(p.GetConversionType(), "nid_lrbg");
}

/**
 * @brief Spec: extracts NID_C (upper 10 bits) and NID_BG (lower 14 bits)
 * from a 24-bit decimal integer.
 *
 * Example: value = (5 << 14) | 1234 = 81234 + 16384*5… let's compute:
 *   NID_C=5, NID_BG=100 → raw = (5 << 14) | 100 = 81920 + 100 = 82020
 */
TEST(NidLrbgPluginTest, ExtractsNidCAndNidBg)
{
    config::NidLrbgPlugin p;
    // NID_C=5, NID_BG=100 → (5 << 14) | 100 = 82020
    std::string result = p.Convert("82020", {});
    EXPECT_NE(result.find("nid_c: 5"),  std::string::npos);
    EXPECT_NE(result.find("nid_bg: 100"), std::string::npos);
}

/**
 * @brief Spec: hex input with "0x" prefix is also accepted.
 */
TEST(NidLrbgPluginTest, AcceptsHexPrefix)
{
    config::NidLrbgPlugin p;
    // 0x014064 = 82020
    std::string result = p.Convert("0x014064", {});
    EXPECT_NE(result.find("nid_c: 5"),  std::string::npos);
    EXPECT_NE(result.find("nid_bg: 100"), std::string::npos);
}

/**
 * @brief Spec: a value > 0xFFFFFF is returned unchanged (out of 24-bit range).
 */
TEST(NidLrbgPluginTest, OutOfRangeReturnedUnchanged)
{
    config::NidLrbgPlugin p;
    EXPECT_EQ(p.Convert("16777216", {}), "16777216"); // 0x1000000
}

/**
 * @brief Spec: an empty string is returned unchanged.
 */
TEST(NidLrbgPluginTest, EmptyReturnedUnchanged)
{
    config::NidLrbgPlugin p;
    EXPECT_EQ(p.Convert("", {}), "");
}

/**
 * @brief Spec: a non-numeric string is returned unchanged (no crash).
 */
TEST(NidLrbgPluginTest, NonNumericReturnedUnchanged)
{
    config::NidLrbgPlugin p;
    EXPECT_EQ(p.Convert("abc-xyz", {}), "abc-xyz");
}

// ============================================================================
// NidEnginePlugin
// ============================================================================

/**
 * @brief Spec: GetConversionType() returns "nid_engine".
 */
TEST(NidEnginePluginTest, GetConversionType)
{
    config::NidEnginePlugin p;
    EXPECT_EQ(p.GetConversionType(), "nid_engine");
}

/**
 * @brief Spec: M_VOLTAGE=1 (AC 25kV 50Hz) maps to the correct string.
 * NID_ENGINE=42, M_VOLTAGE=1 → (1 << 21) | 42 = 2097194
 */
TEST(NidEnginePluginTest, DecodesVoltageAndEngine)
{
    config::NidEnginePlugin p;
    // M_VOLTAGE=1 (AC 25kV 50Hz), NID_ENGINE=42
    unsigned long raw = (1u << 21) | 42u;
    std::string result = p.Convert(std::to_string(raw), {});
    EXPECT_NE(result.find("AC 25kV 50Hz"), std::string::npos);
    EXPECT_NE(result.find("42"),           std::string::npos);
}

/**
 * @brief Spec: M_VOLTAGE=0 maps to "Line not fitted".
 */
TEST(NidEnginePluginTest, VoltageZeroIsLineNotFitted)
{
    config::NidEnginePlugin p;
    std::string result = p.Convert("0", {});  // M_VOLTAGE=0, NID_ENGINE=0
    EXPECT_NE(result.find("Line not fitted"), std::string::npos);
}

/**
 * @brief Spec: values > 0xFFFFFF are returned unchanged.
 */
TEST(NidEnginePluginTest, OutOfRangeReturnedUnchanged)
{
    config::NidEnginePlugin p;
    EXPECT_EQ(p.Convert("16777216", {}), "16777216");
}

/**
 * @brief Spec: a non-numeric string is returned unchanged (no crash).
 */
TEST(NidEnginePluginTest, NonNumericReturnedUnchanged)
{
    config::NidEnginePlugin p;
    EXPECT_EQ(p.Convert("bad", {}), "bad");
}

// ============================================================================
// TrackConditionPlugin
// ============================================================================

/**
 * @brief Spec: GetConversionType() returns "m_trackcond".
 */
TEST(TrackConditionPluginTest, GetConversionType)
{
    config::TrackConditionPlugin p;
    EXPECT_EQ(p.GetConversionType(), "m_trackcond");
}

/**
 * @brief Spec: condition 0 → "Non stopping area (packet 68.0)".
 */
TEST(TrackConditionPluginTest, Condition0IsNonStoppingArea)
{
    config::TrackConditionPlugin p;
    EXPECT_EQ(p.Convert("0", {}), "Non stopping area (packet 68.0)");
}

/**
 * @brief Spec: condition 4 → "Radio hole (packet 68.4)".
 */
TEST(TrackConditionPluginTest, Condition4IsRadioHole)
{
    config::TrackConditionPlugin p;
    EXPECT_EQ(p.Convert("4", {}), "Radio hole (packet 68.4)");
}

/**
 * @brief Spec: condition 11 → "Change of allowed current consumption (packet 68.11)".
 */
TEST(TrackConditionPluginTest, Condition11IsCurrentConsumption)
{
    config::TrackConditionPlugin p;
    EXPECT_EQ(p.Convert("11", {}), "Change of allowed current consumption (packet 68.11)");
}

/**
 * @brief Spec: condition > 11 → "Unknown condition (<value>)".
 */
TEST(TrackConditionPluginTest, UnknownConditionReturnsUnknownString)
{
    config::TrackConditionPlugin p;
    std::string result = p.Convert("99", {});
    EXPECT_NE(result.find("Unknown"), std::string::npos);
    EXPECT_NE(result.find("99"),      std::string::npos);
}

/**
 * @brief Spec: a non-numeric string is returned unchanged (no crash).
 */
TEST(TrackConditionPluginTest, NonNumericReturnedUnchanged)
{
    config::TrackConditionPlugin p;
    EXPECT_EQ(p.Convert("xyz", {}), "xyz");
}

// ============================================================================
// TCyclocPlugin
// ============================================================================

/**
 * @brief Spec: GetConversionType() returns "t_cycloc".
 */
TEST(TCyclocPluginTest, GetConversionType)
{
    config::TCyclocPlugin p;
    EXPECT_EQ(p.GetConversionType(), "t_cycloc");
}

/**
 * @brief Spec: T_CYCLOC = 0 → "No cyclic reporting".
 */
TEST(TCyclocPluginTest, ZeroIsNoCyclicReporting)
{
    config::TCyclocPlugin p;
    EXPECT_EQ(p.Convert("0", {}), "No cyclic reporting");
}

/**
 * @brief Spec: T_CYCLOC = 1 → 10 seconds → "10s (1)".
 */
TEST(TCyclocPluginTest, One10Seconds)
{
    config::TCyclocPlugin p;
    std::string result = p.Convert("1", {});
    EXPECT_NE(result.find("10s"), std::string::npos);
    EXPECT_NE(result.find("(1)"), std::string::npos);
}

/**
 * @brief Spec: T_CYCLOC = 6 → 60 seconds → result includes "1min" and "(6)".
 */
TEST(TCyclocPluginTest, SixProducesOneMinute)
{
    config::TCyclocPlugin p;
    std::string result = p.Convert("6", {});
    EXPECT_NE(result.find("1min"), std::string::npos);
    EXPECT_NE(result.find("(6)"),  std::string::npos);
}

/**
 * @brief Spec: T_CYCLOC = 7 → 70 seconds → result includes "1min" and "10s" and "(7)".
 */
TEST(TCyclocPluginTest, SevenProducesOneMinuteTenSeconds)
{
    config::TCyclocPlugin p;
    std::string result = p.Convert("7", {});
    EXPECT_NE(result.find("1min"), std::string::npos);
    EXPECT_NE(result.find("10s"),  std::string::npos);
    EXPECT_NE(result.find("(7)"),  std::string::npos);
}

/**
 * @brief Spec: a non-numeric string is returned unchanged (no crash).
 */
TEST(TCyclocPluginTest, NonNumericReturnedUnchanged)
{
    config::TCyclocPlugin p;
    EXPECT_EQ(p.Convert("bad", {}), "bad");
}
