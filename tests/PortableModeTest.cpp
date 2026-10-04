// Portable mode: a marker file next to the executable moves all user data to ./data.
#include <gtest/gtest.h>

#include "Config.hpp"
#include "PortableMode.hpp"
#include "qt/utils/AppDataDir.hpp"

#include <QDir>
#include <QSettings>

#include <filesystem>
#include <fstream>

namespace util::portable::test {

class PortableModeTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_dir = std::filesystem::temp_directory_path() / "lv_portable_mode_test";
        std::filesystem::remove_all(m_dir);
        std::filesystem::create_directories(m_dir);
        OverrideExecutableDirForTesting(m_dir);
    }
    void TearDown() override
    {
        OverrideExecutableDirForTesting({});
        std::filesystem::remove_all(m_dir);
    }
    void CreateMarker() { std::ofstream(m_dir / kMarkerFile).put('\n'); }

    std::filesystem::path m_dir;
};

TEST_F(PortableModeTest, IsOffWithoutTheMarkerFile)
{
    EXPECT_FALSE(IsPortable());
    EXPECT_TRUE(DataDir().empty());
}

TEST_F(PortableModeTest, MarkerFileNextToTheExecutableTurnsItOn)
{
    CreateMarker();
    EXPECT_TRUE(IsPortable());
    EXPECT_EQ(DataDir(), m_dir / "data");
}

TEST_F(PortableModeTest, MarkerInAnotherDirectoryIsIgnored)
{
    const auto other = m_dir / "sub";
    std::filesystem::create_directories(other);
    std::ofstream(other / kMarkerFile).put('\n');
    EXPECT_FALSE(IsPortable());
}

#ifdef __APPLE__
TEST_F(PortableModeTest, AppBundleIsNeverPortable)
{
    const auto bundle = m_dir / "LogViewer.app" / "Contents" / "MacOS";
    std::filesystem::create_directories(bundle);
    OverrideExecutableDirForTesting(bundle);
    CreateMarker();
    std::ofstream(bundle / kMarkerFile).put('\n');
    EXPECT_FALSE(IsPortable());
}
#endif

TEST_F(PortableModeTest, ConfigLivesInTheDataFolderAndIsCreated)
{
    CreateMarker();
    const auto appPath = config::GetConfig().GetDefaultAppPath();
    EXPECT_EQ(appPath, m_dir / "data");
    EXPECT_TRUE(std::filesystem::is_directory(m_dir / "data"));
}

// Window state, layouts and scenarios must follow portable mode too; the old
// QSettings("LogViewer", "LogViewer") always went to the registry / plist.
TEST_F(PortableModeTest, AppSettingsLiveInTheDataFolder)
{
    CreateMarker();
    const auto previousFormat = QSettings::defaultFormat();
    ui::qt::utils::UsePortableSettingsIfPortable();
    const QString file = QDir::fromNativeSeparators(ui::qt::utils::AppSettings().fileName());
    const QString dataDir = QDir::fromNativeSeparators(ui::qt::utils::PathToQString(m_dir / "data"));

    // Restore the test-wide settings location chosen in TestMain.cpp.
    OverrideExecutableDirForTesting({});
    QSettings::setDefaultFormat(previousFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
        ui::qt::utils::PathToQString(config::GetConfig().GetDefaultAppPath() / "settings"));

    EXPECT_TRUE(file.startsWith(dataDir)) << file.toStdString();
}

// path::string() is lossy on Windows for characters outside the ANSI code page.
TEST_F(PortableModeTest, DataDirWithNonAsciiCharactersIsReportedExactly)
{
    const auto dir = m_dir / std::filesystem::path(u8"Bj\u00f6rn \u0141\u00f3d\u017a \u65e5\u672c");
    std::filesystem::create_directories(dir);
    OverrideExecutableDirForTesting(dir);
    std::ofstream(dir / kMarkerFile).put('\n');

    const QString expected = QStringLiteral(u"Bj\u00f6rn \u0141\u00f3d\u017a \u65e5\u672c");
    const QString reported = QDir::fromNativeSeparators(ui::qt::utils::AppDataDir());
    EXPECT_TRUE(reported.endsWith(expected + QStringLiteral("/data"))) << reported.toStdString();
}

} // namespace util::portable::test
