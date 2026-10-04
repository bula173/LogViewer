// Hiding a tab from the View > Tabs menu must hide that tab, even after the
// tabs were reordered or another tab was removed.
#include <gtest/gtest.h>
#include <QApplication>
#include <QTabBar>
#include <QTabWidget>
#include <QWidget>

#include "qt/utils/TabVisibility.hpp"

namespace ui::qt::test {

namespace {
void EnsureQApplication()
{
    if (QApplication::instance()) return;
    static int argc = 1;
    static char argv0[] = "tests";
    static char* argv[] = {argv0};
    static QApplication app(argc, argv);
}
} // namespace

TEST(TabVisibilityTest, HidesTheBoundTabAfterTabsAreReordered)
{
    EnsureQApplication();
    QTabWidget tabs;
    auto* a = new QWidget; auto* b = new QWidget; auto* c = new QWidget;
    tabs.addTab(a, "A"); tabs.addTab(b, "B"); tabs.addTab(c, "C");

    QAction action("B");
    action.setCheckable(true);
    action.setChecked(true);
    utils::BindTabVisibilityAction(&action, &tabs, b);

    tabs.tabBar()->moveTab(1, 0); // sort: B, A, C
    action.setChecked(false);     // user unticks "B" in View > Tabs

    EXPECT_FALSE(tabs.tabBar()->isTabVisible(tabs.indexOf(b)));
    EXPECT_TRUE(tabs.tabBar()->isTabVisible(tabs.indexOf(a)));
    EXPECT_TRUE(tabs.tabBar()->isTabVisible(tabs.indexOf(c)));
}

TEST(TabVisibilityTest, HidesTheBoundTabAfterAnEarlierTabIsRemoved)
{
    EnsureQApplication();
    QTabWidget tabs;
    auto* a = new QWidget; auto* b = new QWidget; auto* c = new QWidget;
    tabs.addTab(a, "A"); tabs.addTab(b, "B"); tabs.addTab(c, "C");

    QAction action("C");
    action.setCheckable(true);
    action.setChecked(true);
    utils::BindTabVisibilityAction(&action, &tabs, c);

    tabs.removeTab(tabs.indexOf(a));
    action.setChecked(false);

    EXPECT_FALSE(tabs.tabBar()->isTabVisible(tabs.indexOf(c)));
    EXPECT_TRUE(tabs.tabBar()->isTabVisible(tabs.indexOf(b)));
}

// Opening View > Tabs re-reads each tab's visibility. A plugin tab with the
// same label as a built-in tab must not copy its state onto the built-in one.
TEST(TabVisibilityTest, MenuSyncMatchesTabsByPageNotByLabel)
{
    EnsureQApplication();
    QTabWidget tabs;
    auto* builtIn = new QWidget;
    auto* plugin = new QWidget;
    tabs.addTab(builtIn, "Statistics");
    tabs.addTab(plugin, "Statistics");
    tabs.tabBar()->moveTab(1, 0);                              // plugin tab first
    tabs.tabBar()->setTabVisible(tabs.indexOf(plugin), false); // plugin tab hidden

    QAction action("Statistics");
    action.setCheckable(true);
    action.setChecked(true);
    utils::BindTabVisibilityAction(&action, &tabs, builtIn);

    action.setChecked(false); // stale tick from an earlier menu state
    tabs.tabBar()->setTabVisible(tabs.indexOf(builtIn), true);
    utils::SyncTabVisibilityActions({&action}, &tabs);

    EXPECT_TRUE(action.isChecked());
    EXPECT_TRUE(tabs.tabBar()->isTabVisible(tabs.indexOf(builtIn)));
    EXPECT_FALSE(tabs.tabBar()->isTabVisible(tabs.indexOf(plugin)));
}

} // namespace ui::qt::test
