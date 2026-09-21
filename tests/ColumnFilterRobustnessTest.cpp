// Regression tests for the v1.13.1 audit fixes around sorting, appended events,
// column changes and truncated column-filter lists.
#include <gtest/gtest.h>
#include <QApplication>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>

#include "qt/events/ColumnFilterPopup.hpp"
#include "qt/events/EventsTableModel.hpp"
#include "EventsContainer.hpp"
#include "Config.hpp"

#include <algorithm>
#include <random>

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

// Columns: 0=id 1=level 2=source
class ColumnFilterRobustnessTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        m_saved = config::GetConfig().GetColumns();
        config::GetConfig().GetMutableColumns() = {{"id", true, 50}, {"level", true, 80}, {"source", true, 80}};
        Add(1, "INFO", "a"); Add(2, "ERROR", "a"); Add(3, "INFO", "b");
        Add(4, "ERROR", "b"); Add(5, "WARN", "b");
    }
    void TearDown() override { config::GetConfig().GetMutableColumns() = m_saved; }

    void Add(int id, const char* lvl, const char* src)
    {
        m_events.AddEvent(db::LogEvent(id, {{"level", lvl}, {"source", src}}));
    }
    static QString Cell(const EventsTableModel& m, int r, int c)
    { return m.data(m.index(r, c), Qt::DisplayRole).toString(); }
    static QStringList Column(const EventsTableModel& m, int c)
    {
        QStringList out;
        for (int r = 0; r < m.rowCount(); ++r)
            out << Cell(m, r, c);
        return out;
    }
    static QSet<QString> Set(std::initializer_list<const char*> v)
    { QSet<QString> s; for (auto* x : v) s.insert(x); return s; }

    db::EventsContainer m_events;
    std::vector<config::ColumnConfig> m_saved;
};

TEST_F(ColumnFilterRobustnessTest, AppendedEventsAppearWhenOnlyASortIsActive)
{
    EventsTableModel model(m_events);
    model.sort(0, Qt::DescendingOrder);
    ASSERT_EQ(model.rowCount(), 5);

    Add(6, "INFO", "a");
    model.SyncWithContainer();

    ASSERT_EQ(model.rowCount(), 6);
    EXPECT_EQ(Cell(model, 0, 0), "6"); // new row slotted into the descending sort
}

TEST_F(ColumnFilterRobustnessTest, SortStaysAppliedWhenTheUpstreamFilterChanges)
{
    EventsTableModel model(m_events);
    model.sort(0, Qt::DescendingOrder);
    model.SetFilteredIndices({0, 1, 2});
    EXPECT_EQ(Column(model, 0), (QStringList{"3", "2", "1"}));

    model.ClearFilter();
    EXPECT_EQ(Cell(model, 0, 0), "5");
}

TEST_F(ColumnFilterRobustnessTest, UpstreamFilterWithNoMatchesStaysEmptyWhenSorted)
{
    EventsTableModel model(m_events);
    model.SetFilteredIndices({});
    EXPECT_EQ(model.rowCount(), 0);
    model.sort(0, Qt::DescendingOrder); // the reported bug: this used to bring every row back
    EXPECT_EQ(model.rowCount(), 0);

    model.ClearFilter();
    EXPECT_EQ(model.rowCount(), 5);
}

TEST_F(ColumnFilterRobustnessTest, MixedNumbersAndTextSortIntoAStableTotalOrder)
{
    db::EventsContainer events;
    const char* values[] = {"9", "10", "5.5", "abc", "Abd", "-3", "2.0", "", "nan", "1e3", "7"};
    int id = 1;
    for (int round = 0; round < 20; ++round)          // enough rows to exercise std::sort's
        for (const char* v : values)                  // insertion-sort / partition paths
            events.AddEvent(db::LogEvent(id++, {{"level", v}}));

    EventsTableModel model(events);
    model.sort(1, Qt::AscendingOrder);
    const QStringList asc = Column(model, 1);
    model.sort(1, Qt::DescendingOrder);
    QStringList desc = Column(model, 1);
    std::reverse(desc.begin(), desc.end());
    EXPECT_EQ(asc, desc);

    // Numbers first, in numeric order, then text.
    EXPECT_EQ(asc.first(), "-3");
    const qsizetype i5 = asc.indexOf("5.5"), i9 = asc.indexOf("9"), i10 = asc.indexOf("10");
    EXPECT_LT(i5, i9);
    EXPECT_LT(i9, i10);
    EXPECT_LT(i10, asc.indexOf("abc"));
}

TEST_F(ColumnFilterRobustnessTest, FilterOnAColumnThatDisappearsIsDropped)
{
    EventsTableModel model(m_events);
    model.SetColumnFilter(1, Set({"ERROR"}));
    ASSERT_EQ(model.rowCount(), 2);
    QSignalSpy changed(&model, &EventsTableModel::ColumnFiltersChanged);

    config::GetConfig().GetMutableColumns() = {{"id", true, 50}, {"source", true, 80}};
    model.RefreshColumns();

    EXPECT_FALSE(model.HasAnyColumnFilter());
    EXPECT_EQ(model.rowCount(), 5); // the filter would otherwise hide every row
    EXPECT_EQ(changed.count(), 1);
}

TEST_F(ColumnFilterRobustnessTest, SortFollowsItsColumnWhenColumnsAreReordered)
{
    EventsTableModel model(m_events);
    model.sort(1, Qt::AscendingOrder); // by level

    config::GetConfig().GetMutableColumns() = {{"level", true, 80}, {"id", true, 50}, {"source", true, 80}};
    model.RefreshColumns();
    Add(6, "AAA", "a");
    model.SyncWithContainer();

    EXPECT_EQ(Cell(model, 0, 0), "AAA"); // still sorted by level, now column 0
}

TEST_F(ColumnFilterRobustnessTest, ExcludingFilterKeepsValuesItHasNeverSeen)
{
    EventsTableModel model(m_events);
    model.SetColumnFilterExcluding(1, Set({"INFO"}));
    EXPECT_TRUE(model.HasColumnFilter(1));
    EXPECT_TRUE(model.IsColumnFilterExclusion(1));
    EXPECT_EQ(model.ColumnFilterValues(1), Set({"INFO"}));
    EXPECT_EQ(model.rowCount(), 3);

    Add(6, "NEW", "a");
    model.SyncWithContainer();
    EXPECT_EQ(model.rowCount(), 4); // a whitelist would have hidden the unseen value
}

class ColumnFilterPopupRobustnessTest : public ::testing::Test
{
protected:
    void SetUp() override { EnsureQApplication(); }
};

TEST_F(ColumnFilterPopupRobustnessTest, TruncatedListAppliesAnExclusionNotAWhitelist)
{
    ColumnDistinctValues d;
    d.values = {{"A", 1}, {"B", 1}, {"C", 1}};
    d.truncated = true;
    ColumnFilterPopup p("col", d, std::nullopt);
    QSignalSpy applied(&p, &ColumnFilterPopup::Applied);
    QSignalSpy excluding(&p, &ColumnFilterPopup::AppliedExcluding);

    p.findChild<QListWidget*>("columnFilterList")->item(1)->setCheckState(Qt::Unchecked);
    p.findChild<QPushButton*>("columnFilterOk")->click();

    EXPECT_EQ(applied.count(), 0);
    ASSERT_EQ(excluding.count(), 1);
    EXPECT_EQ(excluding.takeFirst().at(0).value<QSet<QString>>(), (QSet<QString>{"B"}));
}

TEST_F(ColumnFilterPopupRobustnessTest, ValuesContainingPlaceholdersAreShownVerbatim)
{
    ColumnDistinctValues d;
    d.values = {{"could not open %2", 7}};
    ColumnFilterPopup p("col", d, std::nullopt);
    const auto* item = p.findChild<QListWidget*>("columnFilterList")->item(0);
    EXPECT_EQ(item->text(), "could not open %2   (7)");
}

TEST_F(ColumnFilterPopupRobustnessTest, EqualCollatingValuesKeepAStableOrder)
{
    db::EventsContainer events;
    const auto saved = config::GetConfig().GetColumns();
    config::GetConfig().GetMutableColumns() = {{"level", true, 80}};
    events.AddEvent(db::LogEvent(1, {{"level", "error"}}));
    events.AddEvent(db::LogEvent(2, {{"level", "Error"}}));
    EventsTableModel model(events);
    const auto d = model.DistinctColumnValues(0, 10);
    config::GetConfig().GetMutableColumns() = saved;

    ASSERT_EQ(d.values.size(), 2u);
    EXPECT_EQ(d.values[0].value, "Error"); // raw-string tie-break: 'E' < 'e'
    EXPECT_EQ(d.values[1].value, "error");
}

} // namespace ui::qt::test
