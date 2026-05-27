#include <gtest/gtest.h>

#include "dlt/DltParser.hpp"
#include "IDataParser.hpp"
#include "LogEvent.hpp"

#include <sstream>
#include <vector>
#include <cstdint>

namespace parser
{
namespace test
{

// ---------------------------------------------------------------------------
// Shared test observer — collects all events delivered via NewEventBatchFound.
// ---------------------------------------------------------------------------
class DltTestObserver : public IDataParserObserver
{
  public:
    std::vector<db::LogEvent> events;
    int progressCount = 0;

    void ProgressUpdated() override { ++progressCount; }

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

    // Return the value for a field key from the Nth event (0-based).
    std::string Field(size_t eventIdx, const std::string& key) const
    {
        if (eventIdx >= events.size()) return {};
        return events[eventIdx].findByKey(key);
    }
};

// ---------------------------------------------------------------------------
// Low-level byte-building helpers
// ---------------------------------------------------------------------------

static void PushLE16(std::vector<uint8_t>& v, uint16_t val)
{
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

static void PushLE32(std::vector<uint8_t>& v, uint32_t val)
{
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

static void PushBE16(std::vector<uint8_t>& v, uint16_t val)
{
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>(val & 0xFF));
}

static void PushBE32(std::vector<uint8_t>& v, uint32_t val)
{
    v.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>(val & 0xFF));
}

// ---------------------------------------------------------------------------
// BuildMsg — constructs a complete DLT message byte vector.
//
// Parameters:
//   htyp        — standard header flags byte (HTYP_UEH | HTYP_WEID | ...)
//   msgCtr      — 8-bit message counter (byte 1 of standard header)
//   storageSec  — storage header seconds (0 = no storage header)
//   storageUsec — storage header microseconds
//   storageEcu  — storage header ECU ID (exactly 4 bytes, null-padded)
//   weidEcu     — ECU ID to embed in standard body when HTYP_WEID is set
//   sessionId   — BE32 session ID when HTYP_WSID is set (skipped if 0 and not WSID)
//   wtmsVal     — BE32 WTMS value (0.1ms units) when HTYP_WTMS is set
//   msin        — MSIN byte for extended header
//   numArgs     — NOAR byte for extended header
//   apid        — 4-byte AppID (null-padded)
//   ctid        — 4-byte ContextID (null-padded)
//   payload     — raw payload bytes
//   withStorage — whether to prepend 16-byte storage header
// ---------------------------------------------------------------------------

struct MsgSpec
{
    uint8_t  htyp        = 0x01;  // HTYP_UEH by default
    uint8_t  msgCtr      = 0;
    bool     withStorage = true;
    uint32_t storageSec  = 0;
    uint32_t storageUsec = 0;
    uint8_t  storageEcu[4] = {'E','C','U','1'};
    uint8_t  weidEcu[4]    = {'E','C','U','1'};
    uint32_t sessionId     = 0;
    uint32_t wtmsVal       = 0;
    uint8_t  msin          = 0x01; // verbose, Log, Off (level=0)
    uint8_t  numArgs       = 0;
    uint8_t  apid[4]       = {'A','P','P','\0'};
    uint8_t  ctid[4]       = {'C','T','X','\0'};
    std::vector<uint8_t> payload;
};

static std::vector<uint8_t> BuildMsg(const MsgSpec& s)
{
    // Build the body first to compute total length.
    std::vector<uint8_t> body;

    // Optional: ECU ID in standard header
    if (s.htyp & 0x04 /* HTYP_WEID */)
    {
        body.push_back(s.weidEcu[0]);
        body.push_back(s.weidEcu[1]);
        body.push_back(s.weidEcu[2]);
        body.push_back(s.weidEcu[3]);
    }
    // Optional: Session ID (BE32)
    if (s.htyp & 0x08 /* HTYP_WSID */)
    {
        PushBE32(body, s.sessionId);
    }
    // Optional: WTMS timestamp (BE32)
    if (s.htyp & 0x10 /* HTYP_WTMS */)
    {
        PushBE32(body, s.wtmsVal);
    }
    // Optional: Extended header (10 bytes)
    if (s.htyp & 0x01 /* HTYP_UEH */)
    {
        body.push_back(s.msin);            // MSIN
        body.push_back(s.numArgs);         // NOAR
        body.push_back(s.apid[0]);         // AppID byte 0
        body.push_back(s.apid[1]);         // AppID byte 1
        body.push_back(s.apid[2]);         // AppID byte 2
        body.push_back(s.apid[3]);         // AppID byte 3
        body.push_back(s.ctid[0]);         // ContextID byte 0
        body.push_back(s.ctid[1]);         // ContextID byte 1
        body.push_back(s.ctid[2]);         // ContextID byte 2
        body.push_back(s.ctid[3]);         // ContextID byte 3
    }
    // Payload
    body.insert(body.end(), s.payload.begin(), s.payload.end());

    // Standard header: 4 bytes, length = 4 + body
    const uint16_t stdLen = static_cast<uint16_t>(4u + body.size());
    std::vector<uint8_t> msg;

    // Storage header (16 bytes): magic(4) + sec_LE32(4) + usec_LE32(4) + ecu(4)
    if (s.withStorage)
    {
        msg.push_back('D');                // magic[0]
        msg.push_back('L');                // magic[1]
        msg.push_back('T');                // magic[2]
        msg.push_back(0x01);               // magic[3]
        PushLE32(msg, s.storageSec);       // seconds LE32
        PushLE32(msg, s.storageUsec);      // microseconds LE32
        msg.push_back(s.storageEcu[0]);    // ECU ID byte 0
        msg.push_back(s.storageEcu[1]);    // ECU ID byte 1
        msg.push_back(s.storageEcu[2]);    // ECU ID byte 2
        msg.push_back(s.storageEcu[3]);    // ECU ID byte 3
    }

    // Standard header
    msg.push_back(s.htyp);                 // HTYP flags
    msg.push_back(s.msgCtr);               // MCNT
    PushBE16(msg, stdLen);                 // LEN in BE16

    msg.insert(msg.end(), body.begin(), body.end());
    return msg;
}

// Convenience: parse a raw byte vector through DltParser.
static void RunParser(const std::vector<uint8_t>& data, DltTestObserver& obs)
{
    std::string str(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream ss(str);
    DltParser parser;
    parser.RegisterObserver(&obs);
    parser.ParseData(ss);
}

// ---------------------------------------------------------------------------
// Test 1: Empty stream → 0 events, no crash.
//
// Contract: An empty input stream must not produce any events and must not
// throw or invoke undefined behaviour.
// ---------------------------------------------------------------------------
TEST(DltParserTest, EmptyStream_NoEventsNocrash)
{
    DltTestObserver obs;
    DltParser parser;
    parser.RegisterObserver(&obs);
    std::istringstream ss("");
    EXPECT_NO_THROW(parser.ParseData(ss));
    EXPECT_EQ(obs.events.size(), 0u);
}

// ---------------------------------------------------------------------------
// Test 2: Storage header timestamp decoded correctly (sec + usec).
//
// Contract: storageSec=1000, storageUsec=500000 must produce
// timestamp == "1000.500000".  The combined formula is
// ts = storageSec + storageUsec / 1e6.
// ---------------------------------------------------------------------------
TEST(DltParserTest, StorageHeader_TimestampSecPlusUsec)
{
    // sec=1000, usec=500000 → 1000.5 s → "1000.500000"
    MsgSpec s;
    s.storageSec  = 1000;
    s.storageUsec = 500000;
    s.msgCtr      = 0;
    s.msin        = 0x41; // verbose=1, Log, Info (level=4 → bits[7:4]=0100, bits[3:1]=0(Log))
    // MSIN: bit0=verbose(1), bits[3:1]=msgtype(0=Log), bits[7:4]=loglevel(4=Info)
    // 0x41 = 0b0100_0001 → verbose=1, type=0(Log), level=4(Info)
    s.numArgs = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "timestamp"), "1000.500000");
}

// ---------------------------------------------------------------------------
// Test 3: Storage header ECU ID extracted into EcuID field.
//
// Contract: The 4-byte ECU ID in the storage header ('E','C','U','1') must
// appear in the EcuID field.  Null bytes are trimmed; non-null chars kept.
// ---------------------------------------------------------------------------
TEST(DltParserTest, StorageHeader_EcuIdExtracted)
{
    MsgSpec s;
    s.storageEcu[0] = 'E';
    s.storageEcu[1] = 'C';
    s.storageEcu[2] = 'U';
    s.storageEcu[3] = '1';
    // htyp has NO HTYP_WEID (0x04) set, so storageEcu wins
    s.htyp = 0x01; // only UEH
    s.msin = 0x41;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "EcuID"), "ECU1");
}

// ---------------------------------------------------------------------------
// Test 4: MCNT → MsgCtr field with exact decimal value.
//
// Contract: The second byte of the standard header (message counter) must be
// emitted as the decimal string in field "MsgCtr".
// ---------------------------------------------------------------------------
TEST(DltParserTest, StandardHeader_MsgCtrDecimal)
{
    MsgSpec s;
    s.msgCtr = 42;
    s.msin   = 0x41;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "MsgCtr"), "42");
}

// ---------------------------------------------------------------------------
// Test 5: AppID and ContextID extracted from extended header with null-pad
// trimmed correctly.
//
// Contract: "APP\0" → "APP" and "CTX\0" → "CTX".  ReadId4 trims trailing
// null bytes; a non-null-terminated 4-char string stays 4 chars.
// ---------------------------------------------------------------------------
TEST(DltParserTest, ExtendedHeader_AppIdCtxIdNullTrimmed)
{
    MsgSpec s;
    s.htyp    = 0x01; // UEH only
    s.msin    = 0x41; // verbose, Log, Info
    s.numArgs = 0;
    s.apid[0] = 'A'; s.apid[1] = 'P'; s.apid[2] = 'P'; s.apid[3] = '\0';
    s.ctid[0] = 'C'; s.ctid[1] = 'T'; s.ctid[2] = 'X'; s.ctid[3] = '\0';

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "AppID"),     "APP");
    EXPECT_EQ(obs.Field(0, "ContextID"), "CTX");
}

// ---------------------------------------------------------------------------
// Test 5b: Full 4-char ID with no null bytes is kept intact.
//
// Contract: "APID" (4 non-null bytes) → "APID", not truncated.
// ---------------------------------------------------------------------------
TEST(DltParserTest, ExtendedHeader_AppId4CharsNoNull)
{
    MsgSpec s;
    s.htyp    = 0x01;
    s.msin    = 0x41;
    s.numArgs = 0;
    s.apid[0] = 'A'; s.apid[1] = 'P'; s.apid[2] = 'I'; s.apid[3] = 'D';
    s.ctid[0] = 'C'; s.ctid[1] = 'T'; s.ctid[2] = 'X'; s.ctid[3] = 'T';

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "AppID"),     "APID");
    EXPECT_EQ(obs.Field(0, "ContextID"), "CTXT");
}

// ---------------------------------------------------------------------------
// Test 6a: Log level Info (MSIN loglevel=4) maps to "Info".
//
// Contract: bits[7:4] of MSIN encode log level; 4 → "Info".
// ---------------------------------------------------------------------------
TEST(DltParserTest, LogLevel_Info)
{
    MsgSpec s;
    // MSIN = 0x41: verbose(bit0=1), type=Log(bits[3:1]=000), level=Info(bits[7:4]=0100)
    // 0x41 = 0100 0001b
    s.msin    = 0x41;
    s.numArgs = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "level"), "Info");
}

// ---------------------------------------------------------------------------
// Test 6b: Log level Error (MSIN loglevel=2) maps to "Error".
//
// Contract: bits[7:4]=0010 → "Error".
// ---------------------------------------------------------------------------
TEST(DltParserTest, LogLevel_Error)
{
    MsgSpec s;
    // MSIN = 0x21: verbose(1), type=Log(0), level=Error(2)
    // 0x21 = 0010 0001b
    s.msin    = 0x21;
    s.numArgs = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "level"), "Error");
}

// ---------------------------------------------------------------------------
// Test 7: Verbose string argument decoded: TI_STRG with null-terminated "hello\0".
//
// Contract: TI_STRG (bit9 set) → LE16 length, then bytes; trailing null is
// stripped from the result. "hello\0" (len=6) → info = "hello".
// ---------------------------------------------------------------------------
TEST(DltParserTest, VerboseArg_StringNullTerminated)
{
    MsgSpec s;
    s.msin    = 0x41; // verbose, Log, Info
    s.numArgs = 1;

    // Type-info word: TI_STRG = bit9 = 0x200, LE32
    std::vector<uint8_t> payload;
    PushLE32(payload, 0x00000200u); // TI_STRG

    const char str[] = "hello\0"; // 6 bytes incl. null
    PushLE16(payload, 6u);        // LE16 length = 6
    for (int i = 0; i < 6; ++i)
        payload.push_back(static_cast<uint8_t>(str[i]));

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "hello");
}

// ---------------------------------------------------------------------------
// Test 8: Verbose UINT32 (TYLE=3, 4 bytes LE) decoded to decimal string.
//
// Contract: TI_UINT (bit6) | TYLE=3 → 4-byte LE32 value; 12345678 → "12345678".
// ---------------------------------------------------------------------------
TEST(DltParserTest, VerboseArg_Uint32Decimal)
{
    MsgSpec s;
    s.msin    = 0x41;
    s.numArgs = 1;

    std::vector<uint8_t> payload;
    // TI_UINT=0x40, TYLE=3 → 0x00000043
    PushLE32(payload, 0x00000043u); // TI_UINT | TYLE=3
    PushLE32(payload, 12345678u);   // value LE32

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "12345678");
}

// ---------------------------------------------------------------------------
// Test 9a: Verbose BOOL true (byte value 1) decoded to "true".
//
// Contract: TI_BOOL (bit4=0x10) → 1 byte; 1 → "true".
// ---------------------------------------------------------------------------
TEST(DltParserTest, VerboseArg_BoolTrue)
{
    MsgSpec s;
    s.msin    = 0x41;
    s.numArgs = 1;

    std::vector<uint8_t> payload;
    PushLE32(payload, 0x00000010u); // TI_BOOL
    payload.push_back(0x01);        // value = true

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "true");
}

// ---------------------------------------------------------------------------
// Test 9b: Verbose BOOL false (byte value 0) decoded to "false".
//
// Contract: TI_BOOL byte 0 → "false".
// ---------------------------------------------------------------------------
TEST(DltParserTest, VerboseArg_BoolFalse)
{
    MsgSpec s;
    s.msin    = 0x41;
    s.numArgs = 1;

    std::vector<uint8_t> payload;
    PushLE32(payload, 0x00000010u); // TI_BOOL
    payload.push_back(0x00);        // value = false

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "false");
}

// ---------------------------------------------------------------------------
// Test 10: Multiple verbose args: joined with a single space.
//
// Contract: Two TI_STRG args "foo" and "bar" → info = "foo bar".
// Args that decode to a non-empty string are joined with " ".
// ---------------------------------------------------------------------------
TEST(DltParserTest, VerboseArg_MultipleArgsSpaceJoined)
{
    MsgSpec s;
    s.msin    = 0x41;
    s.numArgs = 2;

    std::vector<uint8_t> payload;

    // Arg 1: "foo\0" (len=4, null-terminated → "foo")
    PushLE32(payload, 0x00000200u);
    PushLE16(payload, 4u);
    payload.push_back('f'); payload.push_back('o'); payload.push_back('o');
    payload.push_back('\0');

    // Arg 2: "bar\0" (len=4 → "bar")
    PushLE32(payload, 0x00000200u);
    PushLE16(payload, 4u);
    payload.push_back('b'); payload.push_back('a'); payload.push_back('r');
    payload.push_back('\0');

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "foo bar");
}

// ---------------------------------------------------------------------------
// Test 11: Non-verbose payload → MsgID formatted as "MsgID=0x0000ABCD".
//
// Contract: When MSIN bit0 (verbose) is 0, the first 4 payload bytes are the
// message ID decoded as LE32 and printed as "MsgID=0x%08X".
// ---------------------------------------------------------------------------
TEST(DltParserTest, NonVerbose_MsgIdFormatted)
{
    MsgSpec s;
    // MSIN: bit0=0 (non-verbose), Log, Info
    // 0x40 = 0100 0000b → verbose=0, type=0, level=4(Info)
    s.msin    = 0x40;
    s.numArgs = 0; // ignored in non-verbose mode

    std::vector<uint8_t> payload;
    PushLE32(payload, 0x0000ABCDu); // MsgID LE32

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "MsgID=0x0000ABCD");
}

// ---------------------------------------------------------------------------
// Test 12: WTMS timestamp overrides storage timestamp.
//
// Contract: When HTYP_WTMS (0x10) is set, the timestamp field must equal
// wtmsVal / 10000.0 regardless of storageSec / storageUsec.
// WTMS unit is 0.1 ms, so 10000 units = 1 second → "1.000000".
// ---------------------------------------------------------------------------
TEST(DltParserTest, WtmsTimestamp_OverridesStorageTimestamp)
{
    MsgSpec s;
    s.htyp        = 0x01 | 0x10; // UEH | WTMS
    s.storageSec  = 9999;          // must be overridden
    s.storageUsec = 999999;        // must be overridden
    s.wtmsVal     = 10000u;        // 10000 * 0.1ms = 1.0 s → "1.000000"
    s.msin        = 0x41;
    s.numArgs     = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "timestamp"), "1.000000");
}

// ---------------------------------------------------------------------------
// Test 13: MSBF flag: BE32 UINT32 decoded correctly (big-endian).
//
// Contract: When HTYP_MSBF (0x02) is set, type-info words and argument
// values are read as big-endian.  A BE32 UINT32 = 0x00000064 (100) → "100".
// ---------------------------------------------------------------------------
TEST(DltParserTest, MsbfFlag_BigEndianUint32)
{
    MsgSpec s;
    s.htyp    = 0x01 | 0x02; // UEH | MSBF
    s.msin    = 0x41;
    s.numArgs = 1;

    std::vector<uint8_t> payload;
    // Type-info word in BE32: TI_UINT | TYLE=3 = 0x00000043 big-endian
    PushBE32(payload, 0x00000043u);
    // Value in BE32: 100
    PushBE32(payload, 100u);

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "100");
}

// ---------------------------------------------------------------------------
// Test 14: TI_VARI prefix skipped — value still decoded correctly after
// variable-info block.
//
// Contract: When TI_VARI (bit11=0x800) is set in the type-info word, the
// parser reads LE16 name-len and LE16 unit-len, then skips name+unit bytes,
// then decodes the actual value.  The value (TI_UINT32 = 77) must appear.
// ---------------------------------------------------------------------------
TEST(DltParserTest, VerboseArg_VariPrefixSkipped_ValueDecoded)
{
    MsgSpec s;
    s.msin    = 0x41;
    s.numArgs = 1;

    std::vector<uint8_t> payload;
    // TI = TI_UINT | TYLE=3 | TI_VARI = 0x40 | 0x03 | 0x800 = 0x843
    PushLE32(payload, 0x00000843u);

    // VARI block: name-len=4 (LE16), unit-len=2 (LE16), then name bytes, then unit bytes
    PushLE16(payload, 4u);  // name length = 4
    PushLE16(payload, 2u);  // unit length = 2
    payload.push_back('n'); payload.push_back('a'); payload.push_back('m'); payload.push_back('e'); // 4 name bytes
    payload.push_back('m'); payload.push_back('s');  // 2 unit bytes

    // Actual UINT32 value = 77
    PushLE32(payload, 77u);

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "77");
}

// ---------------------------------------------------------------------------
// Test 15: No storage header (starts directly with standard header) — parsed
// without crash and events are produced.
//
// Contract: A stream whose first 4 bytes are NOT 'D','L','T',0x01 is treated
// as headerless; the parser reads standard headers directly.  A valid minimal
// standard message (UEH, no optional fields) must yield one event.
// ---------------------------------------------------------------------------
TEST(DltParserTest, NoStorageHeader_ParsedSuccessfully)
{
    MsgSpec s;
    s.withStorage = false;
    s.htyp        = 0x01; // UEH, no WEID/WSID/WTMS
    s.msgCtr      = 7;
    s.msin        = 0x41;
    s.numArgs     = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "MsgCtr"), "7");
    // EcuID must be empty — no storage header and no WEID
    EXPECT_EQ(obs.Field(0, "EcuID"), "");
}

// ---------------------------------------------------------------------------
// Test 16: Multiple messages → correct event count and per-event field values.
//
// Contract: N consecutive well-formed DLT messages in one byte stream must
// produce exactly N events.  MsgCtr must differ per event and match what was
// encoded in each message.
// ---------------------------------------------------------------------------
TEST(DltParserTest, MultipleMessages_EventCountAndPerEventFields)
{
    std::vector<uint8_t> stream;

    for (uint8_t ctr = 0; ctr < 3; ++ctr)
    {
        MsgSpec s;
        s.msgCtr = ctr;
        s.msin   = 0x41;
        s.numArgs = 0;
        auto msg = BuildMsg(s);
        stream.insert(stream.end(), msg.begin(), msg.end());
    }

    DltTestObserver obs;
    RunParser(stream, obs);

    ASSERT_EQ(obs.events.size(), 3u);
    EXPECT_EQ(obs.Field(0, "MsgCtr"), "0");
    EXPECT_EQ(obs.Field(1, "MsgCtr"), "1");
    EXPECT_EQ(obs.Field(2, "MsgCtr"), "2");
}

// ---------------------------------------------------------------------------
// Test 17: Truncated standard header (only 2 bytes) → no crash, 0 events.
//
// Contract: A storage header followed by fewer than 4 bytes (only 2) for the
// standard header is malformed.  The parser must not crash and must emit 0
// events.
// ---------------------------------------------------------------------------
TEST(DltParserTest, TruncatedStandardHeader_NoCrashZeroEvents)
{
    // Construct a valid storage header then only 2 bytes of standard header.
    std::vector<uint8_t> data;

    // Storage header (16 bytes)
    data.push_back('D'); data.push_back('L'); data.push_back('T'); data.push_back(0x01);
    PushLE32(data, 100u);  // sec
    PushLE32(data, 0u);    // usec
    data.push_back('E'); data.push_back('C'); data.push_back('U'); data.push_back('1');

    // Truncated standard header: only 2 bytes instead of 4
    data.push_back(0x01); // HTYP
    data.push_back(0x00); // MCNT  (no LEN bytes follow)

    DltTestObserver obs;
    EXPECT_NO_THROW(RunParser(data, obs));
    EXPECT_EQ(obs.events.size(), 0u);
}

// ---------------------------------------------------------------------------
// Test: SessionID field emitted when HTYP_WSID is set and session != 0.
//
// Contract: When HTYP_WSID (0x08) is set and sessionId != 0, the "SessionID"
// field must be present with the decimal value of the BE32 session word.
// ---------------------------------------------------------------------------
TEST(DltParserTest, SessionId_EmittedWhenNonZero)
{
    MsgSpec s;
    s.htyp      = 0x01 | 0x08; // UEH | WSID
    s.sessionId = 42u;
    s.msin      = 0x41;
    s.numArgs   = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "SessionID"), "42");
}

// ---------------------------------------------------------------------------
// Test: SessionID NOT emitted when value is 0.
//
// Contract: A zero session ID must NOT produce a "SessionID" field — the spec
// says "only if WSID and session != 0".
// ---------------------------------------------------------------------------
TEST(DltParserTest, SessionId_NotEmittedWhenZero)
{
    MsgSpec s;
    s.htyp      = 0x01 | 0x08; // UEH | WSID
    s.sessionId = 0u;
    s.msin      = 0x41;
    s.numArgs   = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "SessionID"), ""); // must be absent / empty
}

// ---------------------------------------------------------------------------
// Test: HTYP_WEID overrides storage ECU ID.
//
// Contract: When HTYP_WEID (0x04) is set in the standard header, the 4-byte
// ECU ID embedded in the standard body overrides the storage-header ECU ID.
// ---------------------------------------------------------------------------
TEST(DltParserTest, WeidEcuId_OverridesStorageEcuId)
{
    MsgSpec s;
    s.htyp = 0x01 | 0x04; // UEH | WEID
    s.storageEcu[0] = 'S'; s.storageEcu[1] = 'T'; s.storageEcu[2] = 'R'; s.storageEcu[3] = 'G';
    s.weidEcu[0]    = 'W'; s.weidEcu[1]    = 'E'; s.weidEcu[2]    = 'I'; s.weidEcu[3]    = 'D';
    s.msin    = 0x41;
    s.numArgs = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "EcuID"), "WEID");
}

// ---------------------------------------------------------------------------
// Test: Non-verbose payload with extra bytes appended as hex dump.
//
// Contract: When the non-verbose payload has more than 4 bytes, bytes [4:]
// are appended after "MsgID=0x..." separated by two spaces as uppercase hex.
// ---------------------------------------------------------------------------
TEST(DltParserTest, NonVerbose_MsgIdWithHexSuffix)
{
    MsgSpec s;
    s.msin    = 0x40; // non-verbose, Log, Info
    s.numArgs = 0;

    std::vector<uint8_t> payload;
    PushLE32(payload, 0x00000001u); // MsgID = 1
    payload.push_back(0xAB);       // extra byte 1
    payload.push_back(0xCD);       // extra byte 2

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "MsgID=0x00000001  AB CD");
}

// ---------------------------------------------------------------------------
// Test: Log message type "Log" emitted correctly.
//
// Contract: MSIN bits[3:1] = 0 → msgType = "Log".
// ---------------------------------------------------------------------------
TEST(DltParserTest, MsgType_Log)
{
    MsgSpec s;
    // 0x41 = verbose=1, type=0(Log), level=4(Info)
    s.msin    = 0x41;
    s.numArgs = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "type"), "Log");
}

// ---------------------------------------------------------------------------
// Test: AppTrace message type encoded in MSIN bits[3:1]=1 → "AppTrace".
//
// Contract: MSIN bits[3:1]=001 → msgType = "AppTrace"; level is not "Log"
// type so level stays at default "Info" only when type==Log per spec.
// ---------------------------------------------------------------------------
TEST(DltParserTest, MsgType_AppTrace)
{
    MsgSpec s;
    // MSIN bits[3:1]=001 (AppTrace): 0x03 = 0000_0011b → verbose=1, type=1(AppTrace), level=0
    s.msin    = 0x03;
    s.numArgs = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "type"), "AppTrace");
}

// ---------------------------------------------------------------------------
// Test: Verbose SINT32 (TYLE=3, 4 bytes LE) negative value decoded correctly.
//
// Contract: TI_SINT | TYLE=3 → signed 4-byte LE; -1 (0xFFFFFFFF) → "-1".
// ---------------------------------------------------------------------------
TEST(DltParserTest, VerboseArg_Sint32Negative)
{
    MsgSpec s;
    s.msin    = 0x41;
    s.numArgs = 1;

    std::vector<uint8_t> payload;
    // TI_SINT=0x20, TYLE=3 → 0x23
    PushLE32(payload, 0x00000023u);
    PushLE32(payload, static_cast<uint32_t>(-1)); // 0xFFFFFFFF = -1 as int32

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "-1");
}

// ---------------------------------------------------------------------------
// Test: Verbose raw-data argument (TI_RAWD) emits "<raw N bytes>".
//
// Contract: TI_RAWD (bit10=0x400) → LE16 length then bytes; the decoded
// string must be "<raw 3 bytes>" regardless of the byte values.
// ---------------------------------------------------------------------------
TEST(DltParserTest, VerboseArg_RawData)
{
    MsgSpec s;
    s.msin    = 0x41;
    s.numArgs = 1;

    std::vector<uint8_t> payload;
    PushLE32(payload, 0x00000400u); // TI_RAWD
    PushLE16(payload, 3u);          // length = 3 bytes
    payload.push_back(0x01);
    payload.push_back(0x02);
    payload.push_back(0x03);

    s.payload = payload;
    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "info"), "<raw 3 bytes>");
}

// ---------------------------------------------------------------------------
// Test: Storage header with sec=0 and usec=0 → timestamp "0.000000".
//
// Contract: The zero timestamp must be formatted as "0.000000", not as an
// empty string or omitted field.
// ---------------------------------------------------------------------------
TEST(DltParserTest, StorageHeader_ZeroTimestampFormatted)
{
    MsgSpec s;
    s.storageSec  = 0;
    s.storageUsec = 0;
    s.msin        = 0x41;
    s.numArgs     = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "timestamp"), "0.000000");
}

// ---------------------------------------------------------------------------
// Test: WTMS=0 with no storage header → timestamp "0.000000".
//
// Contract: 0 / 10000.0 = 0.0, formatted as "0.000000".
// ---------------------------------------------------------------------------
TEST(DltParserTest, WtmsZero_TimestampZero)
{
    MsgSpec s;
    s.htyp        = 0x01 | 0x10; // UEH | WTMS
    s.withStorage = false;
    s.wtmsVal     = 0u;
    s.msin        = 0x41;
    s.numArgs     = 0;

    auto data = BuildMsg(s);
    DltTestObserver obs;
    RunParser(data, obs);

    ASSERT_EQ(obs.events.size(), 1u);
    EXPECT_EQ(obs.Field(0, "timestamp"), "0.000000");
}

} // namespace test
} // namespace parser
