#include <gtest/gtest.h>
#include "analyzers/SequenceMessages.hpp"
#include "EventsContainer.hpp"

namespace analyzer::test {

namespace {

ExchangePattern SourceDestPattern()
{
    ExchangePattern p;
    p.mode          = ExchangeMode::Pair;
    p.senderField   = "source";
    p.receiverField = "destination";
    p.labelField    = "event_type";
    return p;
}

void Add(db::EventsContainer& c, int id, const std::string& src, const std::string& dst,
         const std::string& type = "MSG")
{
    c.AddEvent(db::LogEvent(id, {{"source", src}, {"destination", dst}, {"event_type", type}}));
}

} // namespace

TEST(SequenceMessagesTest, PlaceholderReceiversAreNotActors)
{
    db::EventsContainer c;
    Add(c, 1, "a-west", "internal");
    Add(c, 2, "a-west", "b", "DUAL_TRANSFER");
    Add(c, 3, "b", "INTERNAL");
    Add(c, 4, "b", "none");

    const auto msgs = CollectSequenceMessages(c, SourceDestPattern(), 100);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].from, "a-west");
    EXPECT_EQ(msgs[0].to, "b");
    EXPECT_EQ(msgs[0].label, "DUAL_TRANSFER");
    EXPECT_EQ(msgs[0].eventIndex, 1u);
}

TEST(SequenceMessagesTest, MultiActorReceiverIsFannedOut)
{
    db::EventsContainer c;
    Add(c, 1, "il-multi", "c-west,c-east", "ROUTE_RELEASE");

    const auto msgs = CollectSequenceMessages(c, SourceDestPattern(), 100);
    ASSERT_EQ(msgs.size(), 2u);
    EXPECT_EQ(msgs[0].to, "c-west");
    EXPECT_EQ(msgs[1].to, "c-east");
    EXPECT_EQ(msgs[0].eventIndex, msgs[1].eventIndex);
}

TEST(SequenceMessagesTest, LimitCountsMessagesNotEvents)
{
    db::EventsContainer c;
    Add(c, 1, "a", "internal");
    Add(c, 2, "a", "internal");
    Add(c, 3, "a", "b");
    Add(c, 4, "b", "a");
    Add(c, 5, "a", "b");

    // The two skipped local events must not eat into the limit.
    const auto msgs = CollectSequenceMessages(c, SourceDestPattern(), 2);
    ASSERT_EQ(msgs.size(), 2u);
    EXPECT_EQ(msgs[1].from, "b");
}

TEST(SequenceMessagesTest, EventsMissingEitherFieldAreSkipped)
{
    db::EventsContainer c;
    c.AddEvent(db::LogEvent(1, {{"source", "a"}}));
    c.AddEvent(db::LogEvent(2, {{"destination", "b"}}));
    EXPECT_TRUE(CollectSequenceMessages(c, SourceDestPattern(), 10).empty());
}

TEST(SequenceMessagesTest, PatternWithoutBothFieldsYieldsNothing)
{
    db::EventsContainer c;
    Add(c, 1, "a", "b");
    ExchangePattern p;
    p.senderField = "source";
    EXPECT_TRUE(CollectSequenceMessages(c, p, 10).empty());
}

TEST(SequenceMessagesTest, SplitActorListTrimsAndDropsEmpty)
{
    EXPECT_EQ(SplitActorList(" a , b,,c "), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_TRUE(SplitActorList("").empty());
}

TEST(SequenceMessagesTest, PlaceholderMatchIsCaseInsensitiveAndTrimmed)
{
    EXPECT_TRUE(IsPlaceholderActor("Internal"));
    EXPECT_TRUE(IsPlaceholderActor(" none "));
    EXPECT_FALSE(IsPlaceholderActor("robot"));
    EXPECT_FALSE(IsPlaceholderActor("internal-bus"));
}

} // namespace analyzer::test
