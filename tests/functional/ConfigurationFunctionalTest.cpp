#include <gtest/gtest.h>
#include "Config.hpp"
#include "Logger.hpp"
#include <filesystem>
#include <fstream>

namespace functional::tests {

/**
 * Functional Tests: Configuration Workflow
 * Tests config loading, saving, and persistence
 */
class ConfigurationFunctionalTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create temporary config directory
        m_tempDir = std::filesystem::temp_directory_path() / "logviewer_test";
        std::filesystem::create_directories(m_tempDir);
    }

    void TearDown() override
    {
        // Clean up
        std::filesystem::remove_all(m_tempDir);
    }

    std::filesystem::path m_tempDir;
};

/**
 * TEST: Log level can be changed and persists
 */
TEST_F(ConfigurationFunctionalTest, LogLevelChangesPersist)
{
    auto& cfg = config::GetConfig();

    // Set log level to "info"
    cfg.logLevel = "info";
    cfg.SaveConfig();

    // Verify it was saved
    EXPECT_EQ(cfg.logLevel, "info");

    // Load config again (simulating app restart)
    cfg.LoadConfig();

    // Should still be "info"
    EXPECT_EQ(cfg.logLevel, "info");
}

/**
 * TEST: Dictionary file path is saved and loaded correctly
 */
TEST_F(ConfigurationFunctionalTest, DictionaryPathPersists)
{
    auto& cfg = config::GetConfig();
    std::string testPath = "/path/to/dictionary.json";

    // Set dictionary path
    cfg.SetDictionaryFilePath(testPath);
    cfg.SaveConfig();

    // Verify saved
    EXPECT_EQ(cfg.GetDictionaryFilePath(), testPath);

    // Load config again
    cfg.LoadConfig();

    // Should still be set
    EXPECT_EQ(cfg.GetDictionaryFilePath(), testPath);
}

/**
 * TEST: Invalid log level falls back to default
 */
TEST_F(ConfigurationFunctionalTest, InvalidLogLevelFallsBackToDefault)
{
    auto& cfg = config::GetConfig();

    // Set invalid log level
    cfg.logLevel = "INVALID_LEVEL";
    cfg.SaveConfig();

    // Try to convert
    auto level = util::Logger::fromStrLevel(cfg.logLevel);

    // Should not crash, should use a default
    EXPECT_NO_THROW(util::Logger::SetLevel(level));
}

/**
 * TEST: Config file corruption recovery
 */
TEST_F(ConfigurationFunctionalTest, CorruptConfigFileRecovery)
{
    auto& cfg = config::GetConfig();
    std::string originalPath = cfg.GetConfigPath();

    // Write corrupt JSON to config
    {
        std::ofstream file(originalPath);
        file << "{ invalid json ][";
    }

    // Loading should handle gracefully (not crash)
    EXPECT_NO_THROW(cfg.LoadConfig());
}

/**
 * TEST: Multiple rapid config saves don't corrupt
 */
TEST_F(ConfigurationFunctionalTest, RapidConfigSavesDoNotCorrupt)
{
    auto& cfg = config::GetConfig();

    for (int i = 0; i < 10; ++i) {
        cfg.logLevel = "level_" + std::to_string(i);
        EXPECT_NO_THROW(cfg.SaveConfig());
    }

    // Final load should work
    EXPECT_NO_THROW(cfg.LoadConfig());
}

} // namespace functional::tests
