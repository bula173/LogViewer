#include "src/application/xml/xmlParser.hpp"
#include "src/application/config/Config.hpp"
#include "src/application/db/LogEvent.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>

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
        auto& config = config::GetConfig();

        // Get current working directory (assumes running from project root or
        // build/)
        std::filesystem::path cwd = std::filesystem::current_path();
        std::filesystem::path configPath = cwd / "config.json";

        config.SetConfigFilePath(configPath.string());
        config.LoadConfig();

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
        .WillOnce(
            [](const db::LogEvent& event)
            {
                EXPECT_EQ(event.findByKey("timestamp"), "2025-01-14T15:20:55");
                EXPECT_EQ(event.findByKey("type"), "INFO");
                EXPECT_EQ(event.findByKey("info"), "Test event");
            });

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