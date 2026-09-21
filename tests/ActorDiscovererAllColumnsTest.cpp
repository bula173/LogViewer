// Actor discovery offers every column as a potential actor list.
#include <gtest/gtest.h>

#include "ActorDiscoverer.hpp"
#include "EventsContainer.hpp"

#include <algorithm>
#include <string>

namespace analyzer::test {

namespace {
bool Has(const std::vector<std::string>& v, const std::string& s)
{
    return std::find(v.begin(), v.end(), s) != v.end();
}
} // namespace

// A log where every event has the same sender and only "internal" receivers
// (e.g. a robot-only test run): the columns are still actor columns.
TEST(ActorDiscovererAllColumns, SingleValuedSourceAndDestinationAreOffered)
{
    db::EventsContainer events;
    for (int i = 1; i <= 20; ++i)
        events.AddEvent(db::LogEvent(i, {{"source", "robot"}, {"destination", "internal"},
                                         {"level", i % 2 ? "INFO" : "ERROR"}}));

    const auto r = ActorDiscoverer::Discover(events);
    EXPECT_TRUE(Has(r.actorFields, "source"));
    EXPECT_TRUE(Has(r.actorFields, "destination"));
}

TEST(ActorDiscovererAllColumns, ColumnsWithoutActorLikeNamesAreOfferedToo)
{
    db::EventsContainer events;
    for (int i = 1; i <= 30; ++i)
        events.AddEvent(db::LogEvent(i, {{"colour", i % 3 == 0 ? "red" : "blue"},
                                         {"message", "unique message " + std::to_string(i * 1000)}}));

    const auto r = ActorDiscoverer::Discover(events);
    EXPECT_TRUE(Has(r.actorFields, "colour"));   // 2 distinct values, name says nothing
    EXPECT_TRUE(Has(r.actorFields, "message"));  // 30 distinct values: still a candidate
}

TEST(ActorDiscovererAllColumns, ColumnsWithTooManyDistinctValuesAreSkipped)
{
    db::EventsContainer events;
    for (int i = 1; i <= 500; ++i)
        events.AddEvent(db::LogEvent(i, {{"colour", i % 2 ? "red" : "blue"},
                                         {"serial", "S" + std::to_string(i)}}));

    const auto r = ActorDiscoverer::Discover(events, 500);
    EXPECT_TRUE(Has(r.actorFields, "colour"));
    EXPECT_FALSE(Has(r.actorFields, "serial"));
}

TEST(ActorDiscovererAllColumns, NamedColumnsComeFirst)
{
    db::EventsContainer events;
    for (int i = 1; i <= 20; ++i)
        events.AddEvent(db::LogEvent(i, {{"aaa_plain", i % 2 ? "x" : "y"}, {"host", i % 2 ? "h1" : "h2"}}));

    const auto r = ActorDiscoverer::Discover(events);
    ASSERT_GE(r.actorFields.size(), 2u);
    // "host" matches an actor keyword and therefore precedes the alphabetically earlier plain column.
    const auto host  = std::find(r.actorFields.begin(), r.actorFields.end(), "host");
    const auto plain = std::find(r.actorFields.begin(), r.actorFields.end(), "aaa_plain");
    ASSERT_NE(host, r.actorFields.end());
    ASSERT_NE(plain, r.actorFields.end());
    EXPECT_LT(host, plain);
}

TEST(ActorDiscovererAllColumns, MulticastListsAndPlaceholdersAreNotActors)
{
    db::EventsContainer events;
    for (int i = 1; i <= 30; ++i)
        events.AddEvent(db::LogEvent(i, {{"source", i % 2 ? "RBC West" : "RBC East"},
                                         {"destination", i % 3 == 0 ? "internal" : (i % 2 ? "a,b" : "c")}}));

    const auto r = ActorDiscoverer::Discover(events);
    ASSERT_FALSE(r.patterns.empty());
    const auto& actors = r.patterns.front().actors;
    EXPECT_TRUE(actors.count("a"));
    EXPECT_TRUE(actors.count("b"));
    EXPECT_FALSE(actors.count("a,b"));
    EXPECT_FALSE(actors.count("internal"));
}

} // namespace analyzer::test
