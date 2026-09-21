// A long value must be word-wrapped as soon as an item is shown, not only after
// the panel is resized.
#include <gtest/gtest.h>
#include <QApplication>
#include <QHeaderView>
#include <QTableWidget>
#include <QTest>

#include "qt/panels/ItemDetailsView.hpp"
#include "EventsContainer.hpp"

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

TEST(ItemDetailsViewWrapTest, LongValueWrapsWhenTheItemIsLoaded)
{
    EnsureQApplication();
    db::EventsContainer events;
    std::string longText;
    for (int i = 0; i < 150; ++i)
        longText += "word" + std::to_string(i) + " ";
    events.AddEvent(db::LogEvent(1, {{"short", "x"}}));
    events.AddEvent(db::LogEvent(2, {{"message", longText}}));

    ItemDetailsView view(events);
    view.resize(360, 300);
    view.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&view));

    auto* table = view.findChild<QTableWidget*>();
    ASSERT_NE(table, nullptr);

    view.OnCurrentIndexUpdated(0);
    const int oneLineHeight = table->rowHeight(0);

    view.OnCurrentIndexUpdated(1);
    // The value column fits the viewport (no horizontal scrolling needed)...
    EXPECT_LE(table->columnWidth(0) + table->columnWidth(1), table->viewport()->width());
    // ...and the row grew to hold several wrapped lines, with no resize event in between.
    EXPECT_GT(table->rowHeight(0), 2 * oneLineHeight);
}

} // namespace ui::qt::test
