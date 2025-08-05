#include "src/application/xml/xmlParser.hpp"
#include "src/application/config/Config.hpp"
#include "src/application/db/LogEvent.hpp"
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>

class MockObserver : public parser::IDataParserObserver
{
  public:
    MOCK_METHOD(void, NewEventFound, (db::LogEvent && event), (override));
    MOCK_METHOD(void, ProgressUpdated, (), (override));
    MOCK_METHOD1(NewEventBatchFound,
        void(std::vector<std::pair<int, db::LogEvent::EventItems>>&&
                eventBatch));
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

        // Create test config file
        std::ofstream testConfigFile(configPath);
        testConfigFile << R"({
            "default_parser": "xml",
            "logging": {"level": "debug"},
            "parsers": {
                "xml": {
                    "rootElement": "events",
                    "eventElement": "event",
                    "columns": [
                        {"name": "id", "visible": true, "width": 50},
                        {"name": "timestamp", "visible": true, "width": 150}
                    ]
                }
            }
        })";
        testConfigFile.close();

        // Set the path and load
        config.SetConfigFilePath(configPath.string());
        config.LoadConfig();

        parser.RegisterObserver(&mockObserver);

        // Suppress warnings for ProgressUpdated by stating it can be called any
        // number of times. This makes the tests cleaner and focused on the
        // event notifications.
        EXPECT_CALL(mockObserver, ProgressUpdated())
            .Times(testing::AnyNumber());
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

    // Expect NewEventBatchFound to be called once.
    EXPECT_CALL(mockObserver, NewEventBatchFound(testing::_))
        .Times(1)
        .WillOnce(
            [](const std::vector<std::pair<int, db::LogEvent::EventItems>>&
                    batch)
            {
                // Verify the batch contains exactly one event.
                ASSERT_EQ(batch.size(), 1);

                // Construct a LogEvent from the batch data to use helper
                // methods.
                db::LogEvent event(batch[0].first, std::move(batch[0].second));

                // Change GetValueByKey to findByKey to match your LogEvent
                // class
                EXPECT_EQ(event.findByKey("timestamp"), "2025-01-14T15:20:55");
                EXPECT_EQ(event.findByKey("type"), "INFO");
                EXPECT_EQ(event.findByKey("info"), "Test event");
            });

    // This test should not expect NewEventFound anymore.
    EXPECT_CALL(mockObserver, NewEventFound(testing::_)).Times(0);

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

    // For invalid XML, we expect that NO events are found,
    // either individually or in a batch.
    EXPECT_CALL(mockObserver, NewEventFound(testing::_)).Times(0);
    EXPECT_CALL(mockObserver, NewEventBatchFound(testing::_)).Times(0);

    // The parser might throw an exception or simply finish without sending
    // events. This test ensures no event data is dispatched.
    parser.ParseData(input);
}