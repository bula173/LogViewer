#include <gtest/gtest.h>

#include "evlog/EvlogParser.hpp"
#include "evlog/EvlogTemplateRegistry.hpp"
#include "IDataParser.hpp"
#include "LogEvent.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>

namespace parser
{
namespace test
{

// ---------------------------------------------------------------------------
// Test observer
// ---------------------------------------------------------------------------

class EvlogTestObserver : public IDataParserObserver
{
  public:
    std::vector<db::LogEvent> events;

    void ProgressUpdated() override {}

    void NewEventFound(db::LogEvent&& event) override
    {
        events.push_back(std::move(event));
    }

    void NewEventBatchFound(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& batch) override
    {
        for (auto& [id, items] : batch)
            events.emplace_back(id, std::move(items));
    }

    std::string Field(size_t idx, const std::string& key) const
    {
        if (idx >= events.size()) return {};
        return events[idx].findByKey(key);
    }
};

// ---------------------------------------------------------------------------
// Binary record builder — mirrors the 60-byte posix_log_entry layout
// ---------------------------------------------------------------------------

static constexpr size_t kHdrSize = 60;

// All header fields are little-endian 32-bit.
struct EvlogRecord {
    uint32_t recid      {0};
    uint32_t paySize    {0};   // set automatically by Build()
    int32_t  format     {0};   // NODATA=0, BINARY=1, STRING=2, PRINTF=3
    uint32_t eventType  {0};
    int32_t  facility   {8};   // LOG_USER by default
    int32_t  severity   {6};   // Info
    uint32_t uid        {0};
    uint32_t gid        {0};
    int32_t  pid        {1234};
    int32_t  pgrp       {0};
    int32_t  tvSec      {1000000};
    int32_t  tvNsec     {500000000};
    uint32_t flags      {0};
    int32_t  thread_    {0};
    int32_t  processor  {0};
    std::vector<uint8_t> payload;
};

static void PutLE32(std::vector<uint8_t>& buf, uint32_t v)
{
    buf.push_back(v & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >> 24) & 0xFF);
}

static std::vector<uint8_t> BuildRecord(EvlogRecord r)
{
    r.paySize = static_cast<uint32_t>(r.payload.size());

    std::vector<uint8_t> hdr;
    hdr.reserve(kHdrSize);
    PutLE32(hdr, r.recid);
    PutLE32(hdr, r.paySize);
    PutLE32(hdr, static_cast<uint32_t>(r.format));
    PutLE32(hdr, r.eventType);
    PutLE32(hdr, static_cast<uint32_t>(r.facility));
    PutLE32(hdr, static_cast<uint32_t>(r.severity));
    PutLE32(hdr, r.uid);
    PutLE32(hdr, r.gid);
    PutLE32(hdr, static_cast<uint32_t>(r.pid));
    PutLE32(hdr, static_cast<uint32_t>(r.pgrp));
    PutLE32(hdr, static_cast<uint32_t>(r.tvSec));
    PutLE32(hdr, static_cast<uint32_t>(r.tvNsec));
    PutLE32(hdr, r.flags);
    PutLE32(hdr, static_cast<uint32_t>(r.thread_));
    PutLE32(hdr, static_cast<uint32_t>(r.processor));

    hdr.insert(hdr.end(), r.payload.begin(), r.payload.end());
    return hdr;
}

static std::string ToStream(const std::vector<std::vector<uint8_t>>& records)
{
    std::string s;
    for (const auto& r : records)
        s.append(reinterpret_cast<const char*>(r.data()), r.size());
    return s;
}

static void ParseString(EvlogParser& parser, const std::string& data)
{
    std::istringstream ss(data);
    parser.ParseData(ss);
}

// ---------------------------------------------------------------------------
// Tests — empty / edge cases
// ---------------------------------------------------------------------------

TEST(EvlogParserTest, EmptyStream_ProducesNoEvents)
{
    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, "");
    EXPECT_TRUE(obs.events.empty());
}

TEST(EvlogParserTest, TruncatedHeader_NoCrash_NoEvents)
{
    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);

    // Only 20 bytes — far less than the 60-byte header.
    const std::string partial(20, '\x00');
    ParseString(parser, partial);
    EXPECT_TRUE(obs.events.empty());
}

TEST(EvlogParserTest, TruncatedPayload_NoCrash_NoEvents)
{
    EvlogRecord r;
    r.format  = EvlogParser::STRING;
    r.payload = {'H','e','l','l','o','\0'};
    auto rec = BuildRecord(r);
    // Truncate by removing the last 3 payload bytes.
    rec.resize(rec.size() - 3);

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, std::string(reinterpret_cast<const char*>(rec.data()), rec.size()));
    EXPECT_TRUE(obs.events.empty());
}

// ---------------------------------------------------------------------------
// NODATA format
// ---------------------------------------------------------------------------

TEST(EvlogParserTest, NoData_EmitsEventWithNoInfoField)
{
    EvlogRecord r;
    r.format = EvlogParser::NODATA;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_TRUE(obs.Field(0, "info").empty());
}

// ---------------------------------------------------------------------------
// STRING format
// ---------------------------------------------------------------------------

TEST(EvlogParserTest, StringFormat_NullTerminated_InfoMatches)
{
    EvlogRecord r;
    r.format  = EvlogParser::STRING;
    r.payload = {'H','i',' ','t','h','e','r','e','\0'};

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "Hi there");
}

TEST(EvlogParserTest, StringFormat_NoNullTerminator_FullBytesUsed)
{
    EvlogRecord r;
    r.format  = EvlogParser::STRING;
    r.payload = {'A','B','C'};  // no null

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "ABC");
}

TEST(EvlogParserTest, StringFormat_EmptyPayload_EmptyInfo)
{
    EvlogRecord r;
    r.format  = EvlogParser::STRING;
    // empty payload

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_TRUE(obs.Field(0, "info").empty());
}

// ---------------------------------------------------------------------------
// PRINTF format
// ---------------------------------------------------------------------------

TEST(EvlogParserTest, PrintfFormat_FormatStringExtracted)
{
    EvlogRecord r;
    r.format  = EvlogParser::PRINTF;
    const std::string fmt = "pid=%d uid=%d";
    r.payload.insert(r.payload.end(), fmt.begin(), fmt.end());
    r.payload.push_back('\0');  // null terminator for format string

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    const std::string info = obs.Field(0, "info");
    EXPECT_TRUE(info.find("pid=%d uid=%d") != std::string::npos);
}

TEST(EvlogParserTest, PrintfFormat_VarArgsAppendedAsHex)
{
    EvlogRecord r;
    r.format  = EvlogParser::PRINTF;
    const std::string fmt = "val=%d";
    r.payload.insert(r.payload.end(), fmt.begin(), fmt.end());
    r.payload.push_back('\0');
    // Varargs: one LE32 = 42
    r.payload.push_back(42);
    r.payload.push_back(0);
    r.payload.push_back(0);
    r.payload.push_back(0);

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    const std::string info = obs.Field(0, "info");
    // Hex dump of the varargs must appear after the format string.
    EXPECT_TRUE(info.find("[args:") != std::string::npos);
    EXPECT_TRUE(info.find("2A") != std::string::npos);
}

// ---------------------------------------------------------------------------
// BINARY format — hex dump (no template)
// ---------------------------------------------------------------------------

TEST(EvlogParserTest, BinaryNoTemplate_InfoIsHexDump)
{
    EvlogRecord r;
    r.format  = EvlogParser::BINARY;
    r.payload = {0xDE, 0xAD, 0xBE, 0xEF};

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    const std::string info = obs.Field(0, "info");
    EXPECT_TRUE(info.find("DE") != std::string::npos);
    EXPECT_TRUE(info.find("AD") != std::string::npos);
    EXPECT_TRUE(info.find("BE") != std::string::npos);
    EXPECT_TRUE(info.find("EF") != std::string::npos);
}

// ---------------------------------------------------------------------------
// BINARY format — template decoding
// ---------------------------------------------------------------------------

static std::filesystem::path WriteTempFile(const std::string& name,
    const std::string& content)
{
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    f << content;
    return path;
}

TEST(EvlogParserTest, BinaryWithTemplate_Int32Field_DecodedCorrectly)
{
    const auto tmplFile = WriteTempFile("parser_tmpl_int32.t",
        "facility 8\n"
        "event_type 1\n"
        "attributes {\n"
        "  int uid;\n"
        "}\n");

    EvlogRecord r;
    r.format    = EvlogParser::BINARY;
    r.facility  = 8;
    r.eventType = 1;
    // LE32 value = 1001
    r.payload = {0xE9, 0x03, 0x00, 0x00};

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    parser.SetTemplateFile(tmplFile);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "uid=1001");
}

TEST(EvlogParserTest, BinaryWithTemplate_FormatString_Substituted)
{
    const auto tmplFile = WriteTempFile("parser_tmpl_fmt.t",
        "facility 8\n"
        "event_type 2\n"
        "format \"user=%uid% process=%pid%\"\n"
        "attributes {\n"
        "  uint uid;\n"
        "  uint pid;\n"
        "}\n");

    EvlogRecord r;
    r.format    = EvlogParser::BINARY;
    r.facility  = 8;
    r.eventType = 2;
    // uid=500, pid=1234 as LE32
    r.payload = {0xF4,0x01,0x00,0x00, 0xD2,0x04,0x00,0x00};

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    parser.SetTemplateFile(tmplFile);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "user=500 process=1234");
}

TEST(EvlogParserTest, BinaryWithTemplate_FixedSizeString_NullPadded)
{
    const auto tmplFile = WriteTempFile("parser_tmpl_str.t",
        "facility 8\n"
        "event_type 3\n"
        "attributes {\n"
        "  char hostname[8];\n"
        "}\n");

    EvlogRecord r;
    r.format    = EvlogParser::BINARY;
    r.facility  = 8;
    r.eventType = 3;
    // "myhost\0\0" — 8 bytes, null-padded
    r.payload = {'m','y','h','o','s','t','\0','\0'};

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    parser.SetTemplateFile(tmplFile);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "hostname=myhost");
}

TEST(EvlogParserTest, BinaryWithTemplate_NoMatchingTemplate_FallsBackToHexDump)
{
    const auto tmplFile = WriteTempFile("parser_tmpl_nomatch.t",
        "facility 8\n"
        "event_type 99\n");

    EvlogRecord r;
    r.format    = EvlogParser::BINARY;
    r.facility  = 8;
    r.eventType = 1;   // no template for event_type 1
    r.payload   = {0xAA, 0xBB};

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    parser.SetTemplateFile(tmplFile);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    const std::string info = obs.Field(0, "info");
    EXPECT_TRUE(info.find("AA") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Header field extraction
// ---------------------------------------------------------------------------

TEST(EvlogParserTest, Timestamp_ComputedFromTvSecAndTvNsec)
{
    EvlogRecord r;
    r.format  = EvlogParser::NODATA;
    r.tvSec   = 1000;
    r.tvNsec  = 500000000;  // 0.5 s

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    const std::string ts = obs.Field(0, "timestamp");
    ASSERT_FALSE(ts.empty());
    const double v = std::stod(ts);
    EXPECT_NEAR(v, 1000.5, 1e-6);
}

TEST(EvlogParserTest, Severity_InfoLevel6_EmitsInfoString)
{
    EvlogRecord r;
    r.format   = EvlogParser::NODATA;
    r.severity = 6;  // Info

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "level"), "Info");
}

TEST(EvlogParserTest, Severity_ErrorLevel3_EmitsErrorString)
{
    EvlogRecord r;
    r.format   = EvlogParser::NODATA;
    r.severity = 3;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "level"), "Error");
}

TEST(EvlogParserTest, Severity_AllLevels_Mapped)
{
    static const char* kNames[] = {
        "Emergency","Alert","Critical","Error","Warning","Notice","Info","Debug"};

    for (int sev = 0; sev <= 7; ++sev) {
        EvlogRecord r;
        r.format   = EvlogParser::NODATA;
        r.severity = sev;

        EvlogTestObserver obs;
        EvlogParser parser;
        parser.RegisterObserver(&obs);
        ParseString(parser, ToStream({BuildRecord(r)}));

        ASSERT_EQ(obs.events.size(), 1u);
        EXPECT_EQ(obs.Field(0, "level"), kNames[sev]) << "severity=" << sev;
    }
}

TEST(EvlogParserTest, Facility_LogUser_EmitsUserString)
{
    EvlogRecord r;
    r.format   = EvlogParser::NODATA;
    r.facility = 8;  // LOG_USER

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "facility"), "user");
}

TEST(EvlogParserTest, Facility_LogLocal0_Emits_local0)
{
    EvlogRecord r;
    r.format   = EvlogParser::NODATA;
    r.facility = 128;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "facility"), "local0");
}

TEST(EvlogParserTest, EventType_EmittedAsHex)
{
    EvlogRecord r;
    r.format    = EvlogParser::NODATA;
    r.eventType = 0xABCDEF01u;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "event_type"), "0xABCDEF01");
}

TEST(EvlogParserTest, Pid_EmittedAsDecimal)
{
    EvlogRecord r;
    r.format = EvlogParser::NODATA;
    r.pid    = 4567;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "pid"), "4567");
}

TEST(EvlogParserTest, Uid_EmittedAsDecimal)
{
    EvlogRecord r;
    r.format = EvlogParser::NODATA;
    r.uid    = 1000;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "uid"), "1000");
}

TEST(EvlogParserTest, RecId_EmittedAsDecimal)
{
    EvlogRecord r;
    r.format = EvlogParser::NODATA;
    r.recid  = 42;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "recid"), "42");
}

TEST(EvlogParserTest, Cpu_NonZero_EmittedAsCpuField)
{
    EvlogRecord r;
    r.format    = EvlogParser::NODATA;
    r.processor = 3;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "cpu"), "3");
}

TEST(EvlogParserTest, Cpu_Zero_NotEmitted)
{
    EvlogRecord r;
    r.format    = EvlogParser::NODATA;
    r.processor = 0;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_TRUE(obs.Field(0, "cpu").empty());
}

// ---------------------------------------------------------------------------
// Flags field
// ---------------------------------------------------------------------------

TEST(EvlogParserTest, Flags_ZeroFlags_NotEmitted)
{
    EvlogRecord r;
    r.format = EvlogParser::NODATA;
    r.flags  = 0;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_TRUE(obs.Field(0, "flags").empty());
}

TEST(EvlogParserTest, Flags_Truncated_EmittedCorrectly)
{
    EvlogRecord r;
    r.format = EvlogParser::NODATA;
    r.flags  = 0x01;  // F_TRUNCATE

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "flags"), "truncated");
}

TEST(EvlogParserTest, Flags_KernelAndBoot_EmittedCommaJoined)
{
    EvlogRecord r;
    r.format = EvlogParser::NODATA;
    r.flags  = 0x02 | 0x04;  // F_KERNEL | F_BOOT

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream({BuildRecord(r)}));

    ASSERT_EQ(obs.events.size(), 1u);
    const std::string flags = obs.Field(0, "flags");
    EXPECT_TRUE(flags.find("kernel") != std::string::npos);
    EXPECT_TRUE(flags.find("boot") != std::string::npos);
    EXPECT_TRUE(flags.find(',') != std::string::npos);
}

// ---------------------------------------------------------------------------
// Sanity checks — corrupt records rejected
// ---------------------------------------------------------------------------

TEST(EvlogParserTest, SanityCheck_OversizedPayload_RecordRejected)
{
    EvlogRecord r;
    r.format  = EvlogParser::STRING;
    r.payload = {'x'};
    auto rec = BuildRecord(r);
    // Overwrite paySize field at offset 4 with an impossibly large value.
    const uint32_t huge = 0x01000000; // 16 MB — exceeds 64 KB limit
    rec[4] = huge & 0xFF;
    rec[5] = (huge >> 8) & 0xFF;
    rec[6] = (huge >> 16) & 0xFF;
    rec[7] = (huge >> 24) & 0xFF;

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, std::string(reinterpret_cast<const char*>(rec.data()), rec.size()));
    EXPECT_TRUE(obs.events.empty());
}

TEST(EvlogParserTest, SanityCheck_InvalidSeverity_RecordRejected)
{
    EvlogRecord r;
    r.format   = EvlogParser::NODATA;
    r.severity = 99;  // out of range
    const auto rec = BuildRecord(r);

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, std::string(reinterpret_cast<const char*>(rec.data()), rec.size()));
    EXPECT_TRUE(obs.events.empty());
}

TEST(EvlogParserTest, SanityCheck_InvalidFormat_RecordRejected)
{
    EvlogRecord r;
    r.format = 99;  // out of range (NODATA=0 … PRINTF=3)
    const auto rec = BuildRecord(r);

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, std::string(reinterpret_cast<const char*>(rec.data()), rec.size()));
    EXPECT_TRUE(obs.events.empty());
}

// ---------------------------------------------------------------------------
// Multiple records
// ---------------------------------------------------------------------------

TEST(EvlogParserTest, MultipleRecords_AllEmitted)
{
    std::vector<std::vector<uint8_t>> recs;
    for (int i = 0; i < 5; ++i) {
        EvlogRecord r;
        r.format  = EvlogParser::STRING;
        r.recid   = static_cast<uint32_t>(i);
        const std::string msg = "msg" + std::to_string(i);
        r.payload.assign(msg.begin(), msg.end());
        r.payload.push_back('\0');
        recs.push_back(BuildRecord(r));
    }

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream(recs));

    ASSERT_EQ(obs.events.size(), 5u);
    for (size_t i = 0; i < 5; ++i)
        EXPECT_EQ(obs.Field(i, "info"), "msg" + std::to_string(i));
}

TEST(EvlogParserTest, MultipleRecords_EventIdsAreSequential)
{
    std::vector<std::vector<uint8_t>> recs;
    for (int i = 0; i < 3; ++i) {
        EvlogRecord r;
        r.format = EvlogParser::NODATA;
        recs.push_back(BuildRecord(r));
    }

    EvlogTestObserver obs;
    EvlogParser parser;
    parser.RegisterObserver(&obs);
    ParseString(parser, ToStream(recs));

    ASSERT_EQ(obs.events.size(), 3u);
    EXPECT_EQ(obs.events[0].getId(), 0);
    EXPECT_EQ(obs.events[1].getId(), 1);
    EXPECT_EQ(obs.events[2].getId(), 2);
}

} // namespace test
} // namespace parser
