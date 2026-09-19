#include <gtest/gtest.h>
#include <QApplication>

#include "qt/events/EventsTableModel.hpp"
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
} // namespace

// A configured column named "source" must show the event's own "source" data
// field. Previously the model treated that name as the multi-file merge
// alias (LogEvent::GetSource()), which is empty for unmerged events, so the
// column rendered blank.
class EventsTableModelSourceColumnTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        m_savedColumns = config::GetConfig().GetColumns();
        auto& cols = config::GetConfig().GetMutableColumns();
        cols = {{"id", true, 50}, {"source", true, 100}, {"info", true, 100}};
    }

    void TearDown() override
    {
        config::GetConfig().GetMutableColumns() = m_savedColumns;
    }

    QString Cell(const EventsTableModel& model, int row, int col) const
    {
        return model.data(model.index(row, col), Qt::DisplayRole).toString();
    }

    std::vector<config::ColumnConfig> m_savedColumns;
};

TEST_F(EventsTableModelSourceColumnTest, ConfiguredSourceColumnShowsDataField)
{
    db::EventsContainer events;
    events.AddEvent(db::LogEvent(1, {{"source", "a-west"}, {"info", "hello"}}));

    EventsTableModel model(events);
    ASSERT_EQ(model.columnCount(), 3);
    EXPECT_EQ(Cell(model, 0, 1), "a-west");
    EXPECT_EQ(Cell(model, 0, 2), "hello");
}

TEST_F(EventsTableModelSourceColumnTest, MergedEventsKeepAliasInDynamicColumnAndFieldInConfiguredColumn)
{
    db::EventsContainer events;
    db::LogEvent ev(1, {{"source", "a-west"}, {"info", "hello"}});
    ev.SetSource("fileA");
    events.AddEvent(std::move(ev));

    EventsTableModel model(events);
    // id | dynamic merge "source" | configured "source" | info
    ASSERT_EQ(model.columnCount(), 4);
    EXPECT_EQ(Cell(model, 0, 1), "fileA");
    EXPECT_EQ(Cell(model, 0, 2), "a-west");
}

} // namespace ui::qt::test
