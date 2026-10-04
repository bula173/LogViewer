// Reloading a log (container cleared, then refilled) must show the new rows.
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

class EventsReloadTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        m_saved = config::GetConfig().GetColumns();
        config::GetConfig().GetMutableColumns() = {{"id", true, 50}, {"level", true, 80}};
    }
    void TearDown() override { config::GetConfig().GetMutableColumns() = m_saved; }

    void Load(int n)
    {
        for (int i = 1; i <= n; ++i)
            m_events.AddEvent(db::LogEvent(i, {{"level", i % 2 ? "INFO" : "ERROR"}}));
    }

    db::EventsContainer m_events;
    std::vector<config::ColumnConfig> m_saved;
};

TEST_F(EventsReloadTest, RowsAppearAfterReloadWithoutSort)
{
    EventsTableModel model(m_events);
    Load(10);
    model.SyncWithContainer();
    m_events.Clear();
    Load(5);
    model.SyncWithContainer();
    EXPECT_EQ(model.rowCount(), 5);
}

TEST_F(EventsReloadTest, RowsAppearAfterReloadWithActiveSort)
{
    EventsTableModel model(m_events);
    Load(10);
    model.sort(0, Qt::DescendingOrder);
    m_events.Clear();
    Load(5);
    model.SyncWithContainer();
    EXPECT_EQ(model.rowCount(), 5);
}

TEST_F(EventsReloadTest, RowsAppearAfterReloadWithSortAndUpstreamFilter)
{
    EventsTableModel model(m_events);
    Load(10);
    model.SetFilteredIndices({0, 1, 2});
    model.sort(0, Qt::AscendingOrder);
    m_events.Clear();
    Load(5);
    model.ClearFilter();
    model.SyncWithContainer();
    EXPECT_EQ(model.rowCount(), 5);
}

TEST_F(EventsReloadTest, RowsAppearWhenTheViewSortedAnEmptyModelAtStartup)
{
    // QTableView::setSortingEnabled(true) sorts the (still empty) model.
    EventsTableModel model(m_events);
    model.sort(0, Qt::AscendingOrder);
    Load(7);
    model.SyncWithContainer();
    EXPECT_EQ(model.rowCount(), 7);
}

TEST_F(EventsReloadTest, TailAppendsKeepTheSortOrderOfAFullResort)
{
    EventsTableModel model(m_events);
    Load(20);
    model.sort(0, Qt::DescendingOrder);
    for (int batch = 0; batch < 5; ++batch)
    {
        for (int k = 0; k < 7; ++k)
            m_events.AddEvent(db::LogEvent(100 + batch * 7 + k, {{"level", (k % 3) ? "INFO" : "ERROR"}}));
        model.SyncWithContainer();
    }
    // Reference: a full re-sort of the same data.
    std::vector<int> incremental;
    for (int r = 0; r < model.rowCount(); ++r)
        incremental.push_back(model.index(r, 0).data().toInt());
    model.sort(0, Qt::DescendingOrder);
    std::vector<int> full;
    for (int r = 0; r < model.rowCount(); ++r)
        full.push_back(model.index(r, 0).data().toInt());
    EXPECT_EQ(incremental, full);
    EXPECT_EQ(model.rowCount(), 55);
}

} // namespace ui::qt::test
