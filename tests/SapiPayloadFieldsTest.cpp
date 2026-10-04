#include <gtest/gtest.h>
#include "sapi/SapiLogParser.hpp"
#include "LogEvent.hpp"

#include <sstream>
#include <string>
#include <vector>

// Payload field extraction of the safeAPI parser: structured payloads become
// `p.<key>` fields, the `[unit | GP]` info marker becomes `unit`. Payload
// samples are taken verbatim from real merged test logs.

namespace parser::test
{

namespace
{

db::LogEvent::EventItems Extract(std::string_view payload)
{
    db::LogEvent::EventItems out;
    SapiLogParser::ExtractPayloadFields(payload, out);
    return out;
}

using Items = db::LogEvent::EventItems;

class SapiPayloadCollector : public IDataParserObserver
{
public:
    std::vector<db::LogEvent> events;

    void ProgressUpdated() override {}
    void NewEventFound(db::LogEvent&& ev) override { events.push_back(std::move(ev)); }
    void NewEventBatchFound(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& batch) override
    {
        for (auto& [id, items] : batch)
            events.emplace_back(id, std::move(items));
    }
};

} // namespace

// ---------------------------------------------------------------------------
// key=value payloads
// ---------------------------------------------------------------------------

TEST(SapiPayloadFieldsTest, KeyValueStatusPayload)
{
    EXPECT_EQ(Extract("cycle=415 level=INFO site=WEST unit=GW-CTC/WEST up=1"),
        (Items{{"p.cycle", "415"}, {"p.level", "INFO"}, {"p.site", "WEST"},
               {"p.unit", "GW-CTC/WEST"}, {"p.up", "1"}}));
}

TEST(SapiPayloadFieldsTest, RepeatedKeyKeepsFirstOccurrence)
{
    EXPECT_EQ(Extract("cycle=0 site=EAST site=WEST"),
        (Items{{"p.cycle", "0"}, {"p.site", "EAST"}}));
}

TEST(SapiPayloadFieldsTest, BracketedValuesStayWhole)
{
    EXPECT_EQ(Extract("nid_engine=1 packetsPresent=['P3', 'P57'] segments=[{'d': 0, 'g': 1}]"),
        (Items{{"p.nid_engine", "1"}, {"p.packetsPresent", "['P3', 'P57']"},
               {"p.segments", "[{'d': 0, 'g': 1}]"}}));
}

TEST(SapiPayloadFieldsTest, QuotedValuesAreUnquoted)
{
    EXPECT_EQ(Extract(R"(name="Connect Train" note='a b' esc="say \"hi\"")"),
        (Items{{"p.name", "Connect Train"}, {"p.note", "a b"}, {"p.esc", R"(say "hi")"}}));
}

TEST(SapiPayloadFieldsTest, RealLogQuirkTrailingParenIsKeptVerbatim)
{
    // Real line: [DUAL_TRANSFER] ... [cycle=418 level=DEBUG site=WEST CRC=OK)]
    EXPECT_EQ(Extract("cycle=418 level=DEBUG site=WEST CRC=OK)"),
        (Items{{"p.cycle", "418"}, {"p.level", "DEBUG"}, {"p.site", "WEST"}, {"p.CRC", "OK)"}}));
}

TEST(SapiPayloadFieldsTest, BracketGroupsOfPairsAreUnwrappedAndLabelsSkipped)
{
    EXPECT_EQ(Extract("[train_id=1] [P0] [cycle=2 d_lrbg=50 balise=BG_W_TRK1]"),
        (Items{{"p.train_id", "1"}, {"p.cycle", "2"}, {"p.d_lrbg", "50"},
               {"p.balise", "BG_W_TRK1"}}));
}

TEST(SapiPayloadFieldsTest, FreeTextYieldsNoFields)
{
    EXPECT_TRUE(Extract("PONG").empty());
    EXPECT_TRUE(Extract("SET_IL_STATUS up").empty());
    EXPECT_TRUE(Extract("'Assert Ready For Scenario', 'AND', 'Connect Train', '1'").empty());
    // Free text that merely contains pairs is not structured.
    EXPECT_TRUE(Extract("no trains connected yet - up to 100 supported, see connectTrain period=6.0s").empty());
    EXPECT_TRUE(Extract("name=After Cold Switch Route status=PASS").empty());
    EXPECT_TRUE(Extract("M24 for nidEngine 1: expected packet 'P3' present, got packetsPresent=[]").empty());
    EXPECT_TRUE(Extract("[ 6563113] [A/WEST] cycle 418: negotiation ok (role=ONLINE, counterpart=ONLINE)").empty());
    EXPECT_TRUE(Extract("[P0]").empty());
}

TEST(SapiPayloadFieldsTest, MalformedKeyValueYieldsNoFields)
{
    EXPECT_TRUE(Extract("a=1 b=[2").empty());        // unbalanced bracket
    EXPECT_TRUE(Extract("a=1 b=2]").empty());
    EXPECT_TRUE(Extract("a=1 b=\"open").empty());    // unterminated quote
    EXPECT_TRUE(Extract("a=1 b=\"x\"y").empty());    // text after the closing quote
    EXPECT_TRUE(Extract("a=1 b=").empty());          // empty value
    EXPECT_TRUE(Extract("a=1 =2").empty());          // empty key
    EXPECT_TRUE(Extract("a=1 9x=2").empty());        // key must start with a letter or '_'
}

// ---------------------------------------------------------------------------
// JSON payloads
// ---------------------------------------------------------------------------

TEST(SapiPayloadFieldsTest, FlatJsonObjectKeepsSourceOrder)
{
    EXPECT_EQ(Extract(R"({"cmd": "getMessage", "message": "3", "nidEngine": 1, "ok": true, "x": null})"),
        (Items{{"p.cmd", "getMessage"}, {"p.message", "3"}, {"p.nidEngine", "1"},
               {"p.ok", "true"}, {"p.x", "null"}}));
    EXPECT_EQ(Extract(R"({"reason": "no 3 received yet for nidEngine 1", "status": "ERR"})"),
        (Items{{"p.reason", "no 3 received yet for nidEngine 1"}, {"p.status", "ERR"}}));
}

TEST(SapiPayloadFieldsTest, NestedJsonIsFlattenedOneLevelDeep)
{
    EXPECT_EQ(Extract(R"({"cmd": "sendMessage", "fields": {"end_signal": 14, "route_type": 0}, "list": [1, 2]})"),
        (Items{{"p.cmd", "sendMessage"}, {"p.fields.end_signal", "14"},
               {"p.fields.route_type", "0"}, {"p.list", "[1,2]"}}));
    EXPECT_EQ(Extract(R"({"message": "3", "packets": {"15": {"ma_length": 2380, "ma_seq": 1}}, "status": "OK"})"),
        (Items{{"p.message", "3"}, {"p.packets.15", R"({"ma_length":2380,"ma_seq":1})"},
               {"p.status", "OK"}}));
}

TEST(SapiPayloadFieldsTest, InvalidJsonYieldsNoFields)
{
    EXPECT_TRUE(Extract("{'bogus_field': '999'}").empty()); // Python repr, seen in real logs
    EXPECT_TRUE(Extract(R"({"cmd": "x", })").empty());
    EXPECT_TRUE(Extract(R"({"cmd": "x"} trailing)").empty());
    EXPECT_TRUE(Extract("[1, 2, 3]").empty());
}

// ---------------------------------------------------------------------------
// Caps and empty input
// ---------------------------------------------------------------------------

TEST(SapiPayloadFieldsTest, NoPayloadYieldsNoFields)
{
    EXPECT_TRUE(Extract("").empty());
    EXPECT_TRUE(Extract("   ").empty());
}

TEST(SapiPayloadFieldsTest, FieldCountIsCapped)
{
    std::string payload;
    for (int i = 0; i < 50; ++i)
        payload += "k" + std::to_string(i) + "=" + std::to_string(i) + " ";
    const auto out = Extract(payload);
    ASSERT_EQ(out.size(), 32u);
    EXPECT_EQ(out.front(), (std::pair<std::string, std::string>{"p.k0", "0"}));
    EXPECT_EQ(out.back(), (std::pair<std::string, std::string>{"p.k31", "31"}));
}

TEST(SapiPayloadFieldsTest, OverlongKeyOrValueIsDroppedAndHugePayloadSkipped)
{
    const std::string longValue(600, 'v');
    const std::string longKey(70, 'k');
    EXPECT_EQ(Extract("a=1 big=" + longValue + " " + longKey + "=2 b=3"),
        (Items{{"p.a", "1"}, {"p.b", "3"}}));

    std::string huge = "a=1";
    while (huge.size() <= 4096)
        huge += " b=2";
    EXPECT_TRUE(Extract(huge).empty());
}

TEST(SapiPayloadFieldsTest, AppendsAfterExistingItems)
{
    Items out{{"payload", "x=1"}};
    SapiLogParser::ExtractPayloadFields("x=1", out);
    EXPECT_EQ(out, (Items{{"payload", "x=1"}, {"p.x", "1"}}));
}

// ---------------------------------------------------------------------------
// Unit marker
// ---------------------------------------------------------------------------

TEST(SapiPayloadFieldsTest, UnitMarker)
{
    EXPECT_EQ(SapiLogParser::ExtractUnit("[a-west | GP] Log message"), "a-west");
    EXPECT_EQ(SapiLogParser::ExtractUnit("[b-east | RTE] Log message"), "b-east");
    EXPECT_EQ(SapiLogParser::ExtractUnit("[c-east|GP] self"), "c-east");

    EXPECT_EQ(SapiLogParser::ExtractUnit("Control command sent"), "");
    EXPECT_EQ(SapiLogParser::ExtractUnit("[ 419310003] [GW-IL EAST] starting"), "");
    EXPECT_EQ(SapiLogParser::ExtractUnit("[a west | GP] x"), "");
    EXPECT_EQ(SapiLogParser::ExtractUnit("[a-west | ] x"), "");
    EXPECT_EQ(SapiLogParser::ExtractUnit("[ | GP] x"), "");
    EXPECT_EQ(SapiLogParser::ExtractUnit("[a-west | GP x"), "");
    EXPECT_EQ(SapiLogParser::ExtractUnit(""), "");
}

// ---------------------------------------------------------------------------
// Parser level
// ---------------------------------------------------------------------------

TEST(SapiPayloadFieldsTest, ParserEmitsUnitAndPayloadFieldsAndKeepsRawPayload)
{
    const std::string text =
        "# FULL MERGED TEST STEPS, CONTAINER & SIMULATOR LOGS\n"
        "[2026-10-04T05:32:11.000001Z] [GENERAL] [RBC West] [b] [DEBUG] [DUAL_TRANSFER] "
        "[[a-west | GP] sample] [cycle=418 level=DEBUG site=WEST size=343]\n"
        "[2026-10-04T05:32:11.000002Z] [IO] [robot] [Train 1] [INFO] [getMessage] "
        "[Control command sent] [{\"cmd\": \"getMessage\", \"message\": \"3\", \"nidEngine\": 1}]\n"
        "[2026-10-04T05:32:11.000003Z] [GENERAL] [RBC West] [internal] [INFO] [LOG] "
        "[[b-west | GP] Log message] [[   6563125] [B/WEST] cycle 422: negotiation ok (role=ONLINE)]\n"
        "[2026-10-04T05:32:11.000004Z] [IO] [robot] [internal] [INFO] [STEP] [Connect Train] \n";

    SapiPayloadCollector col;
    SapiLogParser        parser;
    parser.RegisterObserver(&col);
    std::istringstream ss(text);
    parser.ParseData(ss);
    ASSERT_EQ(col.events.size(), 4u);

    const auto& status = col.events[0];
    EXPECT_EQ(status.findByKey("payload"), "cycle=418 level=DEBUG site=WEST size=343");
    EXPECT_EQ(status.findByKey("unit"), "a-west");
    EXPECT_EQ(status.findByKey("p.cycle"), "418");
    EXPECT_EQ(status.findByKey("p.size"), "343");
    EXPECT_EQ(status.findByKey("source"), "RBC West");

    const auto& cmd = col.events[1];
    EXPECT_EQ(cmd.findByKey("payload"), R"({"cmd": "getMessage", "message": "3", "nidEngine": 1})");
    EXPECT_EQ(cmd.findByKey("p.cmd"), "getMessage");
    EXPECT_EQ(cmd.findByKey("p.nidEngine"), "1");
    EXPECT_EQ(cmd.findByKey("unit"), "");

    // Free-text log line: unit yes, no invented payload fields.
    const auto& log = col.events[2];
    EXPECT_EQ(log.findByKey("unit"), "b-west");
    EXPECT_EQ(log.getEventItems().size(), 9u); // 8 core fields + unit

    // No payload at all: exactly the 7 fixed fields.
    EXPECT_EQ(col.events[3].getEventItems().size(), 7u);
}

} // namespace parser::test
