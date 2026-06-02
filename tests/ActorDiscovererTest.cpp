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
    EXPECT_FALSE(r.exchange.has_value());
    EXPECT_TRUE(r.actorFields.empty());
}

TEST(ActorDiscoverer, SingleEventNoFromTo)
{
    db::EventsContainer c;
    AddEvent(c, Make({{"level","INFO"},{"message","hello"}}));
    const auto r = ActorDiscoverer::Discover(c);
    EXPECT_FALSE(r.exchange.has_value());
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
    ASSERT_TRUE(r.exchange.has_value());
    EXPECT_EQ(r.exchange->senderField,   "from");
    EXPECT_EQ(r.exchange->receiverField, "to");
    EXPECT_EQ(r.exchange->actors.size(), 2u);
    EXPECT_TRUE(r.exchange->actors.count("ServiceA"));
    EXPECT_TRUE(r.exchange->actors.count("ServiceB"));
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
    ASSERT_TRUE(r.exchange.has_value());
    EXPECT_EQ(r.exchange->senderField,   "source");
    EXPECT_EQ(r.exchange->receiverField, "dest");
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
    ASSERT_TRUE(r.exchange.has_value());
    EXPECT_EQ(r.exchange->labelField, "action");
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
    ASSERT_TRUE(r.exchange.has_value());
    EXPECT_EQ(r.exchange->actors.size(), 3u);
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
    if (r.exchange)
    {
        EXPECT_NE(r.exchange->senderField,   "reqid");
        EXPECT_NE(r.exchange->receiverField, "reqid");
        EXPECT_NE(r.exchange->labelField,    "reqid");
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
    if (r.exchange)
        EXPECT_NE(r.exchange->senderField, r.exchange->receiverField);
}
