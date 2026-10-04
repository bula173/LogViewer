// RefreshView() (config change, reload, merge) must redraw the details even
// when the shown event is the current one; plain appends need not.
#include <gtest/gtest.h>
#include <QApplication>
#include <QTableWidget>

#include "qt/panels/ItemDetailsView.hpp"
#include "EventsContainer.hpp"
#include "Config.hpp"

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

QTableWidgetItem* ValueItem(QTableWidget* table, const QString& key)
{
    for (int r = 0; r < table->rowCount(); ++r)
        if (table->item(r, 0) && table->item(r, 0)->text() == key)
            return table->item(r, 1);
    return nullptr;
}
} // namespace

TEST(ItemDetailsViewReloadTest, RefreshViewRedrawsTheShownEvent)
{
    EnsureQApplication();
    auto& highlights = config::GetConfig().itemHighlights;
    struct Restore {
        config::ItemHighlightMap& map; config::ItemHighlightMap saved;
        ~Restore() { map = saved; }
    } restore {highlights, highlights};

    db::EventsContainer events;
    events.AddEvent(db::LogEvent(1, {{"msg", "hello"}}));
    ItemDetailsView view(events);
    auto* table = view.findChild<QTableWidget*>();

    events.SetCurrentItem(0);
    ASSERT_NE(ValueItem(table, "msg"), nullptr);
    EXPECT_FALSE(ValueItem(table, "msg")->font().bold());

    highlights["msg"].bold = true; // e.g. the settings dialog changed the config
    view.RefreshView();

    ASSERT_NE(ValueItem(table, "msg"), nullptr);
    EXPECT_TRUE(ValueItem(table, "msg")->font().bold());
}

} // namespace ui::qt::test
