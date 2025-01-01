#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "src/application/parser/data_parser.hpp"
#include "src/application/db/event.hpp"

class MockDataParserObserver : public parser::DataParserObserver
{
public:
  MOCK_METHOD(void, ProgressUpdated, (), (const, override));
  MOCK_METHOD(void, NewEventFound, (db::Event && event), (override));
};

class MockDataParser : public parser::DataParser
{
public:
  MOCK_METHOD(void, ParseData, (std::istream & input), (override));
  MOCK_METHOD(uint32_t, GetCurrentProgress, (), (const, override));
  MOCK_METHOD(uint32_t, GetTotalProgress, (), (const, override));
  MOCK_METHOD(db::Event &, GetEvent, (), (const, override));
};

class DataParserTest : public ::testing::Test
{
protected:
  MockDataParser mockParser;
  MockDataParserObserver mockObserver;

  void SetUp() override
  {
    mockParser.RegisterObserver(&mockObserver);
  }
};

TEST_F(DataParserTest, NewEventNotificationTest)
{
  EXPECT_CALL(mockObserver, NewEventFound(testing::_)).Times(1);
  mockParser.NewEventNotification({1, {{"key1", "value1"}, {"key2", "value2"}}});
}

TEST_F(DataParserTest, SendProgressTest)
{
  EXPECT_CALL(mockObserver, ProgressUpdated()).Times(1);
  mockParser.SendProgress();
}
