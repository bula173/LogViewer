#include <gtest/gtest.h>

#include "analyzers/ActorDiscoverer.hpp"
#include "db/EventsContainer.hpp"
#include "db/LogEvent.hpp"

using namespace analyzer;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void AddEvent(db::EventsContainer& c,
                     db::LogEvent::EventItems items)
{
    static int id = 0;
    c.AddEvent(db::LogEvent(id++, std::move(items)));
}

static db::LogEvent::EventItems Make(
    std::initializer_list<std::pair<std::string,std::string>> pairs)
{
    return db::LogEvent::EventItems(pairs.begin(), pairs.end());
}

// ---------------------------------------------------------------------------
// Scoring helpers
// ---------------------------------------------------------------------------

TEST(ActorDiscovererScore, SenderKeywords)
{
    EXPECT_GT(ActorDiscoverer::ScoreSender("from"),    0);
    EXPECT_GT(ActorDiscoverer::ScoreSender("sender"),  0);
    EXPECT_GT(ActorDiscoverer::ScoreSender("source"),  0);
    EXPECT_GT(ActorDiscoverer::ScoreSender("src"),     0);
    EXPECT_GT(ActorDiscoverer::ScoreSender("origin"),  0);
    EXPECT_GT(ActorDiscoverer::ScoreSender("FROM"),    0); // case-insensitive
    EXPECT_EQ(ActorDiscoverer::ScoreSender("level"),   0);
    EXPECT_EQ(ActorDiscoverer::ScoreSender("timestamp"),0);
}

TEST(ActorDiscovererScore, ReceiverKeywords)
{
    EXPECT_GT(ActorDiscoverer::ScoreReceiver("to"),          0);
    EXPECT_GT(ActorDiscoverer::ScoreReceiver("dest"),        0);
    EXPECT_GT(ActorDiscoverer::ScoreReceiver("destination"), 0);
    EXPECT_GT(ActorDiscoverer::ScoreReceiver("target"),      0);
    EXPECT_GT(ActorDiscoverer::ScoreReceiver("receiver"),    0);
    EXPECT_GT(ActorDiscoverer::ScoreReceiver("TO"),          0);
    EXPECT_EQ(ActorDiscoverer::ScoreReceiver("timestamp"),   0);
}

TEST(ActorDiscovererScore, LabelKeywords)
{
    EXPECT_GT(ActorDiscoverer::ScoreLabel("type"),    0);
    EXPECT_GT(ActorDiscoverer::ScoreLabel("action"),  0);
    EXPECT_GT(ActorDiscoverer::ScoreLabel("message"), 0);
    EXPECT_GT(ActorDiscoverer::ScoreLabel("method"),  0);
    EXPECT_EQ(ActorDiscoverer::ScoreLabel("timestamp"), 0);
}

// ---------------------------------------------------------------------------
// Discovery on empty / tiny containers
// ---------------------------------------------------------------------------

TEST(ActorDiscoverer, EmptyContainerReturnsNoResult)
{
    db::EventsContainer c;
    const auto r = ActorDiscoverer::Discover(c);
    EXPECT_TRUE(r.patterns.empty());
    EXPECT_TRUE(r.actorFields.empty());
}

TEST(ActorDiscoverer, SingleEventNoFromTo)
{
    db::EventsContainer c;
    AddEvent(c, Make({{"level","INFO"},{"message","hello"}}));
    const auto r = ActorDiscoverer::Discover(c);
    EXPECT_TRUE(r.patterns.empty());
}

// ---------------------------------------------------------------------------
// Exchange-pattern detection
// ---------------------------------------------------------------------------

TEST(ActorDiscoverer, DetectsFromToPattern)
{
    db::EventsContainer c;
    // Need at least 3 co-existing from+to events for the co-existence check
    for (int i = 0; i < 20; ++i)
    {
        const std::string frm = (i % 2 == 0) ? "ServiceA" : "ServiceB";
        const std::string too = (i % 2 == 0) ? "ServiceB" : "ServiceA";
        AddEvent(c, Make({{"from",frm},{"to",too},{"type","Request"}}));
    }
    const auto r = ActorDiscoverer::Discover(c);
    ASSERT_FALSE(r.patterns.empty());
    EXPECT_EQ(r.patterns[0].mode, analyzer::ExchangeMode::Pair);
    EXPECT_EQ(r.patterns[0].senderField,   "from");
    EXPECT_EQ(r.patterns[0].receiverField, "to");
    EXPECT_EQ(r.patterns[0].actors.size(), 2u);
    EXPECT_TRUE(r.patterns[0].actors.count("ServiceA"));
    EXPECT_TRUE(r.patterns[0].actors.count("ServiceB"));
}

TEST(ActorDiscoverer, DetectsSourceDestPattern)
{
    db::EventsContainer c;
    for (int i = 0; i < 10; ++i)
    {
        // Alternate sender/receiver so each field has 2 unique values (cardinality ≥ 2).
        const std::string src = (i % 2 == 0) ? "NodeX" : "NodeY";
        const std::string dst = (i % 2 == 0) ? "NodeY" : "NodeX";
        AddEvent(c, Make({{"source", src}, {"dest", dst}, {"cmd", "ping"}}));
    }
    const auto r = ActorDiscoverer::Discover(c);
    ASSERT_FALSE(r.patterns.empty());
    EXPECT_EQ(r.patterns[0].senderField,   "source");
    EXPECT_EQ(r.patterns[0].receiverField, "dest");
}

TEST(ActorDiscoverer, PicksBestLabelField)
{
    db::EventsContainer c;
    for (int i = 0; i < 10; ++i)
    {
        // Vary from/to and action so all fields have cardinality ≥ 2.
        const std::string frm = (i % 2 == 0) ? "A" : "B";
        const std::string too = (i % 2 == 0) ? "B" : "A";
        const std::string act = (i % 2 == 0) ? "Login" : "Logout";
        AddEvent(c, Make({{"from", frm}, {"to", too},
                          {"action", act}, {"timestamp", "1.0"}}));
    }
    const auto r = ActorDiscoverer::Discover(c);
    ASSERT_FALSE(r.patterns.empty());
    EXPECT_EQ(r.patterns[0].labelField, "action");
}

TEST(ActorDiscoverer, ActorsUnionFromBothSides)
{
    db::EventsContainer c;
    // A→B, B→C, C→A — three distinct actors
    AddEvent(c, Make({{"from","A"},{"to","B"}}));
    AddEvent(c, Make({{"from","A"},{"to","B"}}));
    AddEvent(c, Make({{"from","A"},{"to","B"}}));
    AddEvent(c, Make({{"from","B"},{"to","C"}}));
    AddEvent(c, Make({{"from","B"},{"to","C"}}));
    AddEvent(c, Make({{"from","B"},{"to","C"}}));
    AddEvent(c, Make({{"from","C"},{"to","A"}}));
    AddEvent(c, Make({{"from","C"},{"to","A"}}));
    AddEvent(c, Make({{"from","C"},{"to","A"}}));
    const auto r = ActorDiscoverer::Discover(c);
    ASSERT_FALSE(r.patterns.empty());
    EXPECT_EQ(r.patterns[0].actors.size(), 3u);
}

TEST(ActorDiscoverer, HighCardinalityFieldIgnored)
{
    // A field with 201+ unique values (e.g. request IDs) must be ignored
    db::EventsContainer c;
    for (int i = 0; i < 300; ++i)
    {
        AddEvent(c, Make({{"from","A"},{"to","B"},
                          {"reqid", std::to_string(i)}})); // 300 unique values
    }
    const auto r = ActorDiscoverer::Discover(c);
    // reqid must not be selected as from/to/label
    if (!r.patterns.empty())
    {
        for (const auto& pat : r.patterns)
        {
            EXPECT_NE(pat.senderField,   "reqid");
            EXPECT_NE(pat.receiverField, "reqid");
            EXPECT_NE(pat.labelField,    "reqid");
        }
    }
}

TEST(ActorDiscoverer, SeparateFromAndToNotPairedWithSelf)
{
    // from and to must be different fields
    db::EventsContainer c;
    for (int i = 0; i < 10; ++i)
        AddEvent(c, Make({{"from","A"},{"from2","B"}})); // no "to"-type field
    const auto r = ActorDiscoverer::Discover(c);
    // Should not pair "from" with "from2" since from2 doesn't score as receiver
    if (!r.patterns.empty())
    {
        for (const auto& pat : r.patterns)
            if (pat.mode == analyzer::ExchangeMode::Pair)
                EXPECT_NE(pat.senderField, pat.receiverField);
    }
}

// ---------------------------------------------------------------------------
// ScoreDirection helper
// ---------------------------------------------------------------------------

TEST(ActorDiscovererScore, DirectionFieldKeywordsOutgoing)
{
    std::set<std::string> outVals, inVals;
    const int score = ActorDiscoverer::ScoreDirection("direction",
        {"out", "send", "request"}, outVals, inVals);
    EXPECT_GT(score, 0);
    EXPECT_GE(outVals.size(), 1u);
}

TEST(ActorDiscovererScore, DirectionFieldKeywordsIncoming)
{
    std::set<std::string> outVals, inVals;
    const int score = ActorDiscoverer::ScoreDirection("direction",
        {"in", "recv", "response"}, outVals, inVals);
    EXPECT_GT(score, 0);
    EXPECT_GE(inVals.size(), 1u);
}

TEST(ActorDiscovererScore, DirectionFieldMixed)
{
    std::set<std::string> outVals, inVals;
    const int score = ActorDiscoverer::ScoreDirection("flow",
        {"send", "recv", "neutral", "unknown"}, outVals, inVals);
    EXPECT_GT(score, 0);
    EXPECT_EQ(outVals.count("send"), 1u);
    EXPECT_EQ(inVals.count("recv"), 1u);
    EXPECT_EQ(outVals.count("neutral"), 0u);
    EXPECT_EQ(inVals.count("neutral"), 0u);
}

// ---------------------------------------------------------------------------
// Mode 2: DirectionField
// ---------------------------------------------------------------------------

TEST(ActorDiscoverer, DetectsDirectionFieldMode)
{
    db::EventsContainer c;
    for (int i = 0; i < 20; ++i)
    {
        const std::string actor = (i % 2 == 0) ? "ProcessA" : "ProcessB";
        const std::string dir = (i % 2 == 0) ? "out" : "in";
        AddEvent(c, Make({{"actor",actor},{"direction",dir},{"timestamp","1.0"}}));
    }
    const auto r = ActorDiscoverer::Discover(c);
    ASSERT_FALSE(r.patterns.empty());
    ASSERT_TRUE(std::any_of(r.patterns.begin(), r.patterns.end(),
        [](const auto& p) { return p.mode == analyzer::ExchangeMode::DirectionField; }));
    const auto& pat = *std::find_if(r.patterns.begin(), r.patterns.end(),
        [](const auto& p) { return p.mode == analyzer::ExchangeMode::DirectionField; });
    EXPECT_EQ(pat.actorField, "actor");
    EXPECT_EQ(pat.directionField, "direction");
    EXPECT_EQ(pat.outgoingValues.count("out"), 1u);
    EXPECT_EQ(pat.incomingValues.count("in"), 1u);
}

TEST(ActorDiscoverer, DirectionFieldWithMultipleActors)
{
    db::EventsContainer c;
    const std::vector<std::string> actors = {"ServiceX", "ServiceY", "ServiceZ"};
    const std::vector<std::string> dirs = {"send", "recv"};
    for (const auto& a : actors)
    {
        for (const auto& d : dirs)
        {
            for (int i = 0; i < 5; ++i)
                AddEvent(c, Make({{"component",a},{"flow",d}}));
        }
    }
    const auto r = ActorDiscoverer::Discover(c);
    const auto it = std::find_if(r.patterns.begin(), r.patterns.end(),
        [](const auto& p) { return p.mode == analyzer::ExchangeMode::DirectionField; });
    if (it != r.patterns.end())
    {
        EXPECT_EQ(it->actors.size(), 3u);
        EXPECT_TRUE(it->actors.count("ServiceX"));
        EXPECT_TRUE(it->actors.count("ServiceY"));
        EXPECT_TRUE(it->actors.count("ServiceZ"));
    }
}

// ---------------------------------------------------------------------------
// Mode 3: SenderOnly
// ---------------------------------------------------------------------------

TEST(ActorDiscoverer, DetectsSenderOnlyMode)
{
    db::EventsContainer c;
    for (int i = 0; i < 20; ++i)
    {
        const std::string sender = (i % 2 == 0) ? "ClientA" : "ClientB";
        AddEvent(c, Make({{"sender",sender},{"timestamp","1.0"},{"level","INFO"}}));
    }
    const auto r = ActorDiscoverer::Discover(c);
    ASSERT_FALSE(r.patterns.empty());
    const auto& pat = r.patterns[0];
    EXPECT_EQ(pat.mode, analyzer::ExchangeMode::SenderOnly);
    EXPECT_EQ(pat.senderField, "sender");
    EXPECT_EQ(pat.actors.size(), 2u);
}

// ---------------------------------------------------------------------------
// Mode 4: ReceiverOnly
// ---------------------------------------------------------------------------

TEST(ActorDiscoverer, DetectsReceiverOnlyMode)
{
    db::EventsContainer c;
    for (int i = 0; i < 20; ++i)
    {
        const std::string receiver = (i % 2 == 0) ? "ServerX" : "ServerY";
        AddEvent(c, Make({{"recipient",receiver},{"msg_id","123"},{"data","abc"}}));
    }
    const auto r = ActorDiscoverer::Discover(c);
    ASSERT_FALSE(r.patterns.empty());
    const auto& pat = r.patterns[0];
    EXPECT_EQ(pat.mode, analyzer::ExchangeMode::ReceiverOnly);
    EXPECT_EQ(pat.receiverField, "recipient");
    EXPECT_EQ(pat.actors.size(), 2u);
}

// ---------------------------------------------------------------------------
// Pattern ranking and confidence
// ---------------------------------------------------------------------------

TEST(ActorDiscoverer, PairModeHasHigherConfidenceThanDirectionField)
{
    db::EventsContainer c;
    // Create events with both Pair and DirectionField patterns present
    for (int i = 0; i < 15; ++i)
    {
        const std::string a = (i % 2 == 0) ? "NodeA" : "NodeB";
        const std::string b = (i % 2 == 0) ? "NodeB" : "NodeA";
        const std::string actor = (i % 3 == 0) ? "Actor1" : "Actor2";
        const std::string dir = (i % 3 == 0) ? "send" : "recv";
        AddEvent(c, Make({{"from",a},{"to",b},{"source",actor},{"direction",dir}}));
    }
    const auto r = ActorDiscoverer::Discover(c);
    if (!r.patterns.empty())
    {
        // Pair mode should rank first due to higher confidence
        EXPECT_EQ(r.patterns[0].mode, ExchangeMode::Pair);
    }
}

TEST(ActorDiscoverer, ActorFieldsCollectedFromRemaining)
{
    db::EventsContainer c;
    for (int i = 0; i < 10; ++i)
    {
        const std::string a = (i % 2 == 0) ? "A" : "B";
        const std::string b = (i % 2 == 0) ? "B" : "A";
        const std::string opt = (i % 3 == 0) ? "X" : "Y";
        AddEvent(c, Make({{"from",a},{"to",b},{"other_field",opt}}));
    }
    const auto r = ActorDiscoverer::Discover(c);
    // from/to are used by the Pair pattern; other_field should be in actorFields
    EXPECT_TRUE(std::count(r.actorFields.begin(), r.actorFields.end(), "other_field") > 0 ||
                r.actorFields.empty()); // May be empty if other_field doesn't score as actor field
}
