#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include "src/application/parser/IDataParser.hpp"
#include "src/application/db/LogEvent.hpp"

class MockIDataParserObserver : public parser::IDataParserObserver
{
public:
  MOCK_METHOD(void, ProgressUpdated, (), (override));
  MOCK_METHOD(void, NewEventFound, (db::LogEvent && event), (override));
};

class MockIDataParser : public parser::IDataParser
{
public:
  MockIDataParser() : IDataParser() {};
  MOCK_METHOD(void, ParseData, (std::istream & input), (override));
  MOCK_METHOD(void, ParseData, (const std::string &input), (override));
  MOCK_METHOD(uint32_t, GetCurrentProgress, (), (const, override));
  MOCK_METHOD(uint32_t, GetTotalProgress, (), (const, override));
};

class IDataParserTest : public ::testing::Test
{
protected:
  MockIDataParser mockParser;
  MockIDataParserObserver mockObserver;
  db::EventsContainer container;

  IDataParserTest() : mockParser(), mockObserver()
  {
  }

  void SetUp() override
  {
    mockParser.RegisterObserver(&mockObserver);
    // Code here will be called immediately after the constructor (right before each test)
  }

  void TearDown() override
  {
    // Code here will be called immediately after each test (right before the destructor)
  }
};

TEST_F(IDataParserTest, NewEventNotificationTest)
{
  db::LogEvent::EventItems eventItems{{"key1", "value1"}, {"key2", "value2"}};
  EXPECT_CALL(mockObserver, NewEventFound(testing::_)).Times(1);
  db::LogEvent event(1, std::move(eventItems));
  mockParser.NewEventNotification(std::move(event));
}

TEST_F(IDataParserTest, SendProgressTest)
{
  EXPECT_CALL(mockObserver, ProgressUpdated()).Times(1);
  mockParser.SendProgress();
}
