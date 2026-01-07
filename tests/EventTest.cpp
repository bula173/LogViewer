#include <gtest/gtest.h>
#include "LogEvent.hpp"

namespace db
{
  class EventTest : public ::testing::Test
  {
  protected:
    using EventItems = std::vector<std::pair<std::string, std::string>>;
    std::unique_ptr<LogEvent> event;
    EventItems eventItems;

    void SetUp() override
    {
      eventItems = {{"key1", "value1"}, {"key2", "value2"}};
      event = std::make_unique<LogEvent>(1, std::move(eventItems));
    }

    void TearDown() override
    {
      event.reset();
    }
  };

  TEST_F(EventTest, ConstructorTest)
  {
    EXPECT_EQ(event->getId(), 1);
    EXPECT_EQ(event->getEventItems().size(), 2);
  }

  TEST_F(EventTest, GetIdTest)
  {
    EXPECT_EQ(event->getId(), 1);
  }

  TEST_F(EventTest, GetEventItemsTest)
  {
    auto items = event->getEventItems();
    EXPECT_EQ(items.size(), 2);
    EXPECT_EQ(items[0].second, "value1");
    EXPECT_EQ(items[1].second, "value2");
  }

  TEST_F(EventTest, FindTest)
  {
    EXPECT_EQ(event->findByKey("key1"), "value1");
    EXPECT_EQ(event->findByKey("key2"), "value2");
    EXPECT_EQ(event->findByKey("key3"), "");
  }

  TEST_F(EventTest, FindNonExistentKeyTest)
  {
    EXPECT_EQ(event->findByKey("nonexistent"), "");
  }

  TEST_F(EventTest, EmptyEventItemsTest)
  {
    LogEvent emptyEvent(2, {});
    EXPECT_EQ(emptyEvent.getId(), 2);
    EXPECT_TRUE(emptyEvent.getEventItems().empty());
    EXPECT_EQ(emptyEvent.findByKey("key1"), "");
  }

  TEST_F(EventTest, DuplicateKeysTest)
  {
    LogEvent::EventItems duplicateItems = {{"key1", "value1"}, {"key1", "value2"}};
    LogEvent duplicateEvent(3, std::move(duplicateItems));
    EXPECT_EQ(duplicateEvent.getId(), 3);
    EXPECT_EQ(duplicateEvent.getEventItems().size(), 2);
    EXPECT_EQ(duplicateEvent.findByKey("key1"), "value1");
  }

  TEST_F(EventTest, FindInEventTest)
  {
    LogEvent::EventItems items = {{"key1", "value1"}, {"key2", "value2"}};
    LogEvent event(1, std::move(items));
    auto it = event.findInEvent("value1");
    EXPECT_NE(it, event.getEventItems().end());
    EXPECT_EQ(it->first, "key1");
    EXPECT_EQ(it->second, "value1");

    it = event.findInEvent("value2");
    EXPECT_NE(it, event.getEventItems().end());
    EXPECT_EQ(it->first, "key2");
    EXPECT_EQ(it->second, "value2");

    it = event.findInEvent("nonexistent");
    EXPECT_EQ(it, event.getEventItems().end());
  }

  TEST_F(EventTest, FindInEventRegexTest)
  {
    LogEvent::EventItems items = {{"key1", "value1"}, {"key2", "value2"}, {"key3", "anotherValue"}};
    LogEvent event(1, std::move(items));

    auto it = event.findInEvent("value[0-9]");
    EXPECT_NE(it, event.getEventItems().end());
    EXPECT_EQ(it->first, "key1");
    EXPECT_EQ(it->second, "value1");
  }

  /**
   * @test Test SetOriginalId method
   * 
   * Verifies that SetOriginalId stores the original_id field correctly
   * and that it can be retrieved later.
   */
  TEST_F(EventTest, SetOriginalIdTest)
  {
    LogEvent event(1, {{"key1", "value1"}});
    
    // Store original ID
    event.SetOriginalId(42);
    
    // Verify it's stored as event data
    EXPECT_EQ(event.findByKey("original_id"), "42");
    
    // Verify the actual event ID hasn't changed
    EXPECT_EQ(event.getId(), 1);
  }

  /**
   * @test Test SetOriginalId overwrites existing original_id
   * 
   * Verifies that calling SetOriginalId multiple times overwrites
   * the previous original_id without creating duplicates.
   */
  TEST_F(EventTest, SetOriginalIdOverwriteTest)
  {
    LogEvent event(1, {{"key1", "value1"}});
    
    // Set original ID first time
    event.SetOriginalId(42);
    EXPECT_EQ(event.findByKey("original_id"), "42");
    
    // Set original ID second time (should overwrite)
    event.SetOriginalId(99);
    EXPECT_EQ(event.findByKey("original_id"), "99");
    
    // Verify no duplicates - should have same number of items
    // Original: 1 item (key1), after first SetOriginalId: 2 items
    // After second SetOriginalId: should still be 2 items (not 3)
    EXPECT_EQ(event.getEventItems().size(), 2);
  }

  /**
   * @test Test SetOriginalId with empty event
   * 
   * Verifies that SetOriginalId works correctly on events with no other data.
   */
  TEST_F(EventTest, SetOriginalIdEmptyEventTest)
  {
    LogEvent event(5, {});
    
    event.SetOriginalId(100);
    
    EXPECT_EQ(event.getId(), 5);
    EXPECT_EQ(event.findByKey("original_id"), "100");
    EXPECT_EQ(event.getEventItems().size(), 1);
  }

}
