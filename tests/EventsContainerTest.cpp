#include <gtest/gtest.h>
#include "src/application/db/events_container.hpp"
#include "src/application/db/event.hpp"

TEST(EventsContainerTest, AddEvent)
{
    db::EventsContainer container;
    container.AddEvent(db::Event(1, {{"key1", "value1"}, {"key2", "value2"}}));
    ASSERT_EQ(container.GetEvent(0), db::Event(1, {{"key1", "value1"}, {"key2", "value2"}}));
}

TEST(EventsContainerTest, GetEvent)
{
    db::EventsContainer container;
    container.AddEvent({1, {{"key1", "value1"}, {"key2", "value2"}}});
    container.AddEvent({2, {{"key3", "value3"}, {"key4", "value4"}}});
    ASSERT_EQ(container.GetEvent(1), db::Event(2, {{"key3", "value3"}, {"key4", "value4"}}));
}
