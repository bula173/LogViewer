// Config load/save robustness:
//  - Reloading config.json (Edit Config, Load from file) must replace the
//    columns, colours, highlights, column order and widths, not append to
//    them (the table showed every column twice and saved them back).
//  - A config.json that is not valid JSON must not be silently replaced by the
//    in-memory defaults on the next save: it is backed up first.
//  - Saving replaces the file in one step (no truncated file, no temp left).
#include <gtest/gtest.h>

#include "Config.hpp"
#include "Version.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace config::test
{

class ConfigPersistenceTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto& cfg = GetConfig();
        m_savedPath = cfg.GetConfigFilePath();
        m_savedColumns = cfg.columns;
        m_savedOrder = cfg.columnOrder;
        m_savedWidths = cfg.columnWidths;
        m_savedColors = cfg.columnColors;
        m_savedHighlights = cfg.itemHighlights;
        m_savedLogLevel = cfg.logLevel;
        m_savedTypeField = cfg.typeFilterField;
        m_savedActorField = cfg.actorField;
        m_savedUpdates = cfg.updates;
        m_savedVersion = cfg.GetConfigVersion();

        m_dir = std::filesystem::temp_directory_path() / "lv_config_persistence_test";
        std::filesystem::remove_all(m_dir);
        std::filesystem::create_directories(m_dir);
        m_file = m_dir / "config.json";
        cfg.SetConfigFilePath(m_file.string());
    }

    void TearDown() override
    {
        auto& cfg = GetConfig();
        cfg.SetConfigFilePath(m_savedPath);
        cfg.columns = m_savedColumns;
        cfg.columnOrder = m_savedOrder;
        cfg.columnWidths = m_savedWidths;
        cfg.columnColors = m_savedColors;
        cfg.itemHighlights = m_savedHighlights;
        cfg.logLevel = m_savedLogLevel;
        cfg.typeFilterField = m_savedTypeField;
        cfg.actorField = m_savedActorField;
        cfg.updates = m_savedUpdates;
        cfg.SetConfigVersion(m_savedVersion);
        std::filesystem::remove_all(m_dir);
    }

    static std::string VersionJson()
    {
        const auto& v = Version::current();
        return R"("version": {"major": )" + std::to_string(v.major) + R"(, "minor": )" +
            std::to_string(v.minor) + R"(, "patch": )" + std::to_string(v.patch) +
            R"(, "type": ")" + v.type + R"("})";
    }

    void WriteFile(const std::string& text) const
    {
        std::ofstream(m_file, std::ios::trunc) << text;
    }

    void WriteValidConfig() const
    {
        WriteFile("{" + VersionJson() + R"(,
            "logging": {"level": "info"},
            "parsers": {"xml": {"columns": [
                {"name": "timestamp", "visible": true, "width": 150},
                {"name": "type", "visible": true, "width": 80},
                {"name": "info", "visible": false, "width": 300}
            ]}},
            "columnColors": {"type": {"ERROR": ["#ff0000", "#000000"]}},
            "itemHighlights": {"info": {"bold": true}},
            "ui": {"columnOrder": ["timestamp", "type", "info"],
                   "columnWidths": {"timestamp": 150, "type": 80}}
        })");
    }

    static std::string ReadFile(const std::filesystem::path& p)
    {
        std::ifstream in(p);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }

    std::filesystem::path m_dir;
    std::filesystem::path m_file;

  private:
    std::string m_savedPath;
    std::vector<ColumnConfig> m_savedColumns;
    std::vector<std::string> m_savedOrder;
    std::map<std::string, int> m_savedWidths;
    ColumnColorMap m_savedColors;
    ItemHighlightMap m_savedHighlights;
    std::string m_savedLogLevel;
    std::string m_savedTypeField;
    std::string m_savedActorField;
    Config::UpdateSettings m_savedUpdates;
    Version::Version m_savedVersion;
};

TEST_F(ConfigPersistenceTest, ReloadReplacesColumnsInsteadOfAppending)
{
    WriteValidConfig();
    auto& cfg = GetConfig();

    cfg.LoadConfig();
    cfg.Reload();

    ASSERT_EQ(cfg.columns.size(), 3u);
    EXPECT_EQ(cfg.columns[0].name, "timestamp");
    EXPECT_EQ(cfg.columns[2].name, "info");
    EXPECT_EQ(cfg.columnOrder.size(), 3u);
    EXPECT_EQ(cfg.columnWidths.size(), 2u);
    EXPECT_EQ(cfg.columnColors.size(), 1u);
    EXPECT_EQ(cfg.itemHighlights.size(), 1u);
}

TEST_F(ConfigPersistenceTest, ReloadDropsEntriesTheNewFileNoLongerHas)
{
    WriteValidConfig();
    auto& cfg = GetConfig();
    cfg.LoadConfig();

    WriteFile("{" + VersionJson() + R"(,
        "parsers": {"xml": {"columns": [{"name": "message"}]}}
    })");
    cfg.LoadConfig();

    ASSERT_EQ(cfg.columns.size(), 1u);
    EXPECT_EQ(cfg.columns[0].name, "message");
    EXPECT_TRUE(cfg.columnOrder.empty());
    EXPECT_TRUE(cfg.columnWidths.empty());
    EXPECT_TRUE(cfg.columnColors.empty());
    EXPECT_TRUE(cfg.itemHighlights.empty());
}

TEST_F(ConfigPersistenceTest, MalformedConfigIsBackedUpBeforeItIsOverwritten)
{
    WriteValidConfig();
    auto& cfg = GetConfig();
    cfg.LoadConfig();

    const std::string broken = "{ \"logging\": { \"level\": \"info\" }, oops";
    WriteFile(broken);
    EXPECT_ANY_THROW(cfg.LoadConfig());
    EXPECT_EQ(cfg.columns.size(), 3u) << "a failed load keeps the settings in memory";

    cfg.SaveConfig(); // e.g. the update check stores its timestamp

    const auto backup = std::filesystem::path(m_file.string() + ".bak");
    ASSERT_TRUE(std::filesystem::exists(backup));
    EXPECT_EQ(ReadFile(backup), broken);
    EXPECT_NO_THROW((void)json::parse(ReadFile(m_file)));

    // Later saves no longer touch the backup.
    cfg.columns.pop_back();
    cfg.SaveConfig();
    EXPECT_EQ(ReadFile(backup), broken);
}

TEST_F(ConfigPersistenceTest, SaveWritesCompleteJsonAndLeavesNoTempFile)
{
    WriteValidConfig();
    auto& cfg = GetConfig();
    cfg.LoadConfig();

    cfg.SaveConfig();

    EXPECT_FALSE(std::filesystem::exists(m_file.string() + ".tmp"));
    EXPECT_FALSE(std::filesystem::exists(m_file.string() + ".bak"));
    const auto saved = json::parse(ReadFile(m_file));
    EXPECT_EQ(saved["parsers"]["xml"]["columns"].size(), 3u);
}

} // namespace config::test
