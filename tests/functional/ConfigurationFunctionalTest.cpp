#include <gtest/gtest.h>
#include "application/config/Config.hpp"
#include "application/util/Logger.hpp"
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
 * TEST: Config can be loaded and saved without crashing
 */
TEST_F(ConfigurationFunctionalTest, ConfigLoadAndSaveDoNotCrash)
{
    auto& cfg = config::GetConfig();

    // Should not crash
    EXPECT_NO_THROW(cfg.LoadConfig());
    EXPECT_NO_THROW(cfg.SaveConfig());
}

/**
 * TEST: Multiple rapid config saves don't crash
 */
TEST_F(ConfigurationFunctionalTest, RapidConfigSavesDoNotCrash)
{
    auto& cfg = config::GetConfig();

    for (int i = 0; i < 10; ++i) {
        cfg.logLevel = (i % 2 == 0) ? "debug" : "info";
        EXPECT_NO_THROW(cfg.SaveConfig());
    }

    // Final load should work
    EXPECT_NO_THROW(cfg.LoadConfig());
}

/**
 * TEST: Config logLevel property is accessible
 */
TEST_F(ConfigurationFunctionalTest, LogLevelPropertyAccessible)
{
    auto& cfg = config::GetConfig();

    // Should be able to read and write logLevel
    std::string originalLevel = cfg.logLevel;
    cfg.logLevel = "trace";
    EXPECT_EQ(cfg.logLevel, "trace");

    // Restore
    cfg.logLevel = originalLevel;
}

} // namespace functional::tests
