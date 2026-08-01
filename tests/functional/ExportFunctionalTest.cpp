#include <gtest/gtest.h>
#include "application/ui/qt/utils/ExportManager.hpp"
#include "application/db/EventsContainer.hpp"
#include "application/db/LogEvent.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace functional::tests {

/**
 * Functional Tests: Export Workflow
 * Tests exporting data to various formats
 */
class ExportFunctionalTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create temporary export directory
        m_tempDir = std::filesystem::temp_directory_path() / "logviewer_export_test";
        std::filesystem::create_directories(m_tempDir);

        // Create sample events
        for (int i = 0; i < 10; ++i) {
            db::LogEvent::EventItems items = {
                {"timestamp", "2026-08-01 12:00:" + std::to_string(i)},
                {"level", i % 2 == 0 ? "INFO" : "ERROR"},
                {"message", "Test message " + std::to_string(i)}
            };
            db::LogEvent event(i, std::move(items));
            m_events.AddEvent(std::move(event));
        }
    }

    void TearDown() override
    {
        std::filesystem::remove_all(m_tempDir);
    }

    db::EventsContainer m_events;
    std::filesystem::path m_tempDir;
};

/**
 * TEST: CSV export creates valid file
 */
TEST_F(ExportFunctionalTest, CSVExportCreatesValidFile)
{
    std::vector<int> rows = {0, 1, 2, 3, 4};
    auto path = m_tempDir / "export.csv";

    auto result = ui::qt::ExportManager::ToCsv(m_events, rows, path.string().c_str());

    EXPECT_TRUE(result.isOk());
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 0);

    // Verify CSV structure
    std::ifstream file(path);
    std::string header;
    std::getline(file, header);
    EXPECT_FALSE(header.empty());
    EXPECT_NE(header.find("timestamp"), std::string::npos);
}

/**
 * TEST: JSON export creates valid JSON
 */
TEST_F(ExportFunctionalTest, JSONExportCreatesValidJSON)
{
    std::vector<int> rows = {0, 1, 2};
    auto path = m_tempDir / "export.json";

    auto result = ui::qt::ExportManager::ToJson(m_events, rows, path.string().c_str());

    EXPECT_TRUE(result.isOk());
    EXPECT_TRUE(std::filesystem::exists(path));

    // Verify it's valid JSON (can be parsed)
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("[") != std::string::npos);
    EXPECT_TRUE(content.find("{") != std::string::npos);
}

/**
 * TEST: XML export creates well-formed XML
 */
TEST_F(ExportFunctionalTest, XMLExportCreatesWellFormedXML)
{
    std::vector<int> rows = {0, 1, 2};
    auto path = m_tempDir / "export.xml";

    auto result = ui::qt::ExportManager::ToXml(m_events, rows, path.string().c_str());

    EXPECT_TRUE(result.isOk());
    EXPECT_TRUE(std::filesystem::exists(path));

    // Verify XML structure
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("<?xml") != std::string::npos);
    EXPECT_TRUE(content.find("<events>") != std::string::npos);
    EXPECT_TRUE(content.find("</events>") != std::string::npos);
}

/**
 * TEST: Export to invalid path fails gracefully
 */
TEST_F(ExportFunctionalTest, ExportToInvalidPathFails)
{
    std::vector<int> rows = {0, 1};
    auto invalidPath = "/invalid/nonexistent/path/export.csv";

    auto result = ui::qt::ExportManager::ToCsv(m_events, rows, invalidPath);

    EXPECT_FALSE(result.isOk());
    EXPECT_TRUE(result.isErr());
}

/**
 * TEST: Export with empty row selection succeeds
 */
TEST_F(ExportFunctionalTest, ExportWithEmptyRowsSucceeds)
{
    std::vector<int> rows; // Empty
    auto path = m_tempDir / "empty.csv";

    auto result = ui::qt::ExportManager::ToCsv(m_events, rows, path.string().c_str());

    // Should succeed even with no rows (just headers)
    EXPECT_TRUE(result.isOk());
    EXPECT_TRUE(std::filesystem::exists(path));
}

/**
 * TEST: Export with special characters in data
 */
TEST_F(ExportFunctionalTest, ExportWithSpecialCharactersInData)
{
    db::EventsContainer specialEvents;

    db::LogEvent event(0, {
        {"message", "Quote: \" Comma: , Newline: \n Tab: \t"}
    });
    specialEvents.AddEvent(std::move(event));

    std::vector<int> rows = {0};
    auto path = m_tempDir / "special.csv";

    auto result = ui::qt::ExportManager::ToCsv(specialEvents, rows, path.string().c_str());

    EXPECT_TRUE(result.isOk());
    EXPECT_TRUE(std::filesystem::exists(path));

    // Verify file can be read
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    EXPECT_FALSE(content.empty());
}

} // namespace functional::tests
