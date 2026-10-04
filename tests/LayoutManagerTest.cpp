// Saved layouts must survive a restart whatever their name: a '/' or '\' in a
// layout name (or in a tab label) used to become a nested QSettings group, so
// the layout silently vanished on the next start.
// QSettings writes to the temporary INI store set up in TestMain.cpp.
#include <gtest/gtest.h>

#include "qt/panels/LayoutManager.hpp"
#include "qt/utils/AppDataDir.hpp"

namespace ui::qt::test {

class LayoutManagerTest : public ::testing::Test
{
  protected:
    void SetUp() override { ClearStore(); }
    void TearDown() override { ClearStore(); }

    static void ClearStore()
    {
        utils::AppSettings s;
        s.remove("userLayouts");
    }
};

TEST_F(LayoutManagerTest, LayoutNamesWithSlashesSurviveARestart)
{
    LayoutDescriptor d;
    d.name = "CAN / J1939 \\ night shift";
    d.activeTab = "Signals";
    d.bottomDockVisible = true;
    d.tabVisibility = {{"Events", true}, {"AI/ML", false}};
    d.windowState = QByteArray("state-bytes");
    {
        LayoutManager manager;
        manager.Save(d);
    }

    LayoutManager restarted;
    ASSERT_EQ(restarted.UserLayouts().size(), 1u);
    const auto& loaded = restarted.UserLayouts().front();
    EXPECT_EQ(loaded.name, d.name);
    EXPECT_EQ(loaded.activeTab, d.activeTab);
    EXPECT_TRUE(loaded.bottomDockVisible);
    EXPECT_EQ(loaded.windowState, d.windowState);
    EXPECT_EQ(loaded.tabVisibility, d.tabVisibility);
}

TEST_F(LayoutManagerTest, NamesDifferingOnlyInCaseStaySeparate)
{
    {
        LayoutManager manager;
        LayoutDescriptor a;
        a.name = "Debug";
        a.activeTab = "Events";
        manager.Save(a);
        LayoutDescriptor b;
        b.name = "debug";
        b.activeTab = "Timeline";
        manager.Save(b);
    }

    LayoutManager restarted;
    ASSERT_EQ(restarted.UserLayouts().size(), 2u);
    EXPECT_EQ(restarted.UserLayouts()[0].activeTab, "Events");
    EXPECT_EQ(restarted.UserLayouts()[1].activeTab, "Timeline");
}

TEST_F(LayoutManagerTest, LayoutsSavedByOlderVersionsAreStillLoaded)
{
    {
        utils::AppSettings s;
        s.beginGroup("userLayouts");
        s.beginGroup("Old layout");
        s.setValue("activeTab", "Patterns");
        s.beginGroup("tabs");
        s.setValue("Events", true);
        s.endGroup();
        s.endGroup();
        s.setValue("names", QStringList{"Old layout"});
        s.endGroup();
    }

    LayoutManager manager;
    ASSERT_EQ(manager.UserLayouts().size(), 1u);
    EXPECT_EQ(manager.UserLayouts().front().name, "Old layout");
    EXPECT_EQ(manager.UserLayouts().front().activeTab, "Patterns");
    EXPECT_TRUE(manager.UserLayouts().front().tabVisibility.value("Events"));

    // Saving converts the store to the new format.
    manager.Remove("Old layout");
    LayoutManager restarted;
    EXPECT_TRUE(restarted.UserLayouts().empty());
}

} // namespace ui::qt::test
