// After View > Side by Side the Events tab's stack shows the comparison page.
// Loading a file normally (or Go To on a bookmark) must bring the events table
// back, otherwise the Events tab shows empty tables while other tabs show data.
#include <gtest/gtest.h>
#include <QApplication>
#include <QLabel>
#include <QStackedWidget>
#include <QTabWidget>

#include "qt/MainWindow.hpp"

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

TEST(MainWindowEventsPageTest, ShowsTheTablePageInsteadOfSideBySide)
{
    EnsureQApplication();
    QTabWidget tabs;
    tabs.addTab(new QLabel("dashboard"), "Dashboard");
    auto* stack = new QStackedWidget;
    stack->addWidget(new QLabel("table"));        // page 0
    stack->addWidget(new QLabel("side by side")); // page 1
    tabs.addTab(stack, "Events");
    tabs.setCurrentIndex(0);
    stack->setCurrentIndex(1);

    MainWindow::ShowEventsTablePage(stack); // load path: page only
    EXPECT_EQ(stack->currentIndex(), 0);
    EXPECT_EQ(tabs.currentIndex(), 0);

    stack->setCurrentIndex(1);
    MainWindow::ShowEventsTablePage(stack, &tabs); // bookmark Go To: also the tab
    EXPECT_EQ(stack->currentIndex(), 0);
    EXPECT_EQ(tabs.currentWidget(), stack);

    MainWindow::ShowEventsTablePage(nullptr, &tabs); // no stack: no crash
}

} // namespace ui::qt::test
