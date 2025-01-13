#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include "src/application/xml/XmlParser.hpp"
#include "src/application/db/LogEvent.hpp"

class MockObserver : public parser::IDataParserObserver
{
public:
  MOCK_METHOD(void, NewEventFound, (db::LogEvent && event), (override));
  MOCK_METHOD(void, ProgressUpdated, (), (override));
};

class XmlParserTest : public ::testing::Test
{
protected:
  MockObserver mockObserver;
  parser::XmlParser parser;

  void SetUp() override
  {
    // Set up code if needed
    parser.RegisterObserver(&mockObserver);
  }

  void TearDown() override
  {
    // Clean up code if needed
  }
};

TEST_F(XmlParserTest, ParseValidXml)
{
  std::string xmlData = R"(
        <events>
            <event>
                <timestamp>2025-01-14T15:20:55</timestamp>
                <type>INFO</type>
                <info>Test event</info>
            </event>
        </events>
    )";

  std::istringstream input(xmlData);

  EXPECT_CALL(mockObserver, NewEventFound(testing::_))
      .Times(1)
      .WillOnce([](const db::LogEvent &event)
                {
            EXPECT_EQ(event.findByKey("timestamp"), "2025-01-14T15:20:55");
            EXPECT_EQ(event.findByKey("type"), "INFO");
            EXPECT_EQ(event.findByKey("info"), "Test event"); });

  parser.ParseData(input);
}

TEST_F(XmlParserTest, ParseInvalidXml)
{
  std::string xmlData = R"(
        <events>
            <event>
                <timestamp>2025-01-14T15:20:55</timestamp>
                <type>INFO</type>
                <info>Test event</info>
            <!-- Missing closing tags for event and events -->
    )";

  std::istringstream input(xmlData);

  EXPECT_CALL(mockObserver, NewEventFound(testing::_)).Times(0);

  parser.ParseData(input);
}