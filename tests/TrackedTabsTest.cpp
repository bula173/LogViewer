// Tools > Reload Plugins must remove only the plugin tabs. It used to remove
// every content tab after the first one, deleting the built-in Statistics,
// Timeline, ... pages that MainWindow still points at (crash on next use).
#include <gtest/gtest.h>

#include <QApplication>
#include <QPointer>
#include <QTabBar>
#include <QTabWidget>
#include <QWidget>

#include "qt/utils/TrackedTabs.hpp"

namespace ui::qt::test {
namespace {

void EnsureQApplication()
{
    if (QApplication::instance())
        return;
    static int argc = 1;
    static char argv0[] = "tests";
    static char* argv[] = {argv0};
    static QApplication app(argc, argv);
}

} // namespace

TEST(TrackedTabsTest, RemovesOnlyTrackedPluginTabs)
{
    EnsureQApplication();
    QTabWidget tabs;
    auto* events = new QWidget;
    auto* stats = new QWidget;
    auto* pluginA = new QWidget;
    auto* timeline = new QWidget;
    auto* pluginB = new QWidget;
    tabs.addTab(events, "Events");
    tabs.addTab(stats, "Statistics");
    tabs.addTab(pluginA, "AI");
    tabs.addTab(timeline, "Timeline");
    tabs.addTab(pluginB, "Other plugin");
    tabs.tabBar()->moveTab(4, 0); // user dragged a plugin tab to the front

    QPointer<QWidget> a(pluginA);
    QPointer<QWidget> b(pluginB);
    std::map<std::string, QWidget*> tracked{{"a", pluginA}, {"b", pluginB}};

    utils::RemoveTrackedTabs(&tabs, tracked);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    EXPECT_TRUE(tracked.empty());
    ASSERT_EQ(tabs.count(), 3);
    EXPECT_GE(tabs.indexOf(events), 0);
    EXPECT_GE(tabs.indexOf(stats), 0);
    EXPECT_GE(tabs.indexOf(timeline), 0);
    EXPECT_TRUE(a.isNull());
    EXPECT_TRUE(b.isNull());
}

TEST(TrackedTabsTest, IgnoresPagesAlreadyRemovedFromTheTabs)
{
    EnsureQApplication();
    QTabWidget tabs;
    auto* events = new QWidget;
    tabs.addTab(events, "Events");
    auto* orphan = new QWidget(&tabs); // tracked but no longer a tab
    QPointer<QWidget> o(orphan);
    std::map<std::string, QWidget*> tracked{{"x", orphan}};

    utils::RemoveTrackedTabs(&tabs, tracked);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    EXPECT_EQ(tabs.count(), 1);
    EXPECT_TRUE(o.isNull());
}

} // namespace ui::qt::test
