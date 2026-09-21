// Portable mode: a marker file next to the executable moves all user data to ./data.
#include <gtest/gtest.h>

#include "Config.hpp"
#include "PortableMode.hpp"

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

TEST_F(PortableModeTest, ConfigLivesInTheDataFolderAndIsCreated)
{
    CreateMarker();
    const auto appPath = config::GetConfig().GetDefaultAppPath();
    EXPECT_EQ(appPath, m_dir / "data");
    EXPECT_TRUE(std::filesystem::is_directory(m_dir / "data"));
}

} // namespace util::portable::test
