#include <gtest/gtest.h>
#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTableView>
#include <QTest>

#include "qt/events/ColumnFilterPopup.hpp"
#include "qt/events/EventsTableModel.hpp"
#include "qt/events/EventsTableView.hpp"
#include "qt/events/FilterHeaderView.hpp"
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

// Columns: 0=id 1=level 2=source
class ColumnFilterTest : public ::testing::Test
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
    static QSet<QString> Set(std::initializer_list<const char*> v)
    { QSet<QString> s; for (auto* x : v) s.insert(x); return s; }

    db::EventsContainer m_events;
    std::vector<config::ColumnConfig> m_saved;
};

TEST_F(ColumnFilterTest, DistinctValuesAreSortedWithCounts)
{
    EventsTableModel model(m_events);
    const auto d = model.DistinctColumnValues(1, 100);
    ASSERT_EQ(d.values.size(), 3u);
    EXPECT_EQ(d.values[0].value, "ERROR"); EXPECT_EQ(d.values[0].count, 2);
    EXPECT_EQ(d.values[1].value, "INFO");  EXPECT_EQ(d.values[1].count, 2);
    EXPECT_EQ(d.values[2].value, "WARN");  EXPECT_EQ(d.values[2].count, 1);
    EXPECT_FALSE(d.truncated);
}

TEST_F(ColumnFilterTest, DistinctValuesReportTruncation)
{
    EventsTableModel model(m_events);
    const auto d = model.DistinctColumnValues(0, 2); // 5 distinct ids, cap 2
    EXPECT_EQ(d.values.size(), 2u);
    EXPECT_TRUE(d.truncated);
}

TEST_F(ColumnFilterTest, SingleColumnFilterKeepsMatchingRows)
{
    EventsTableModel model(m_events);
    model.SetColumnFilter(1, Set({"ERROR"}));
    ASSERT_EQ(model.rowCount(), 2);
    EXPECT_EQ(Cell(model, 0, 0), "2");
    EXPECT_EQ(Cell(model, 1, 0), "4");
    EXPECT_TRUE(model.HasColumnFilter(1));
    EXPECT_FALSE(model.HasColumnFilter(2));
}

TEST_F(ColumnFilterTest, FiltersOnDifferentColumnsAreAnded)
{
    EventsTableModel model(m_events);
    model.SetColumnFilter(1, Set({"ERROR", "INFO"}));
    model.SetColumnFilter(2, Set({"b"}));
    ASSERT_EQ(model.rowCount(), 2); // ids 3 (INFO,b) and 4 (ERROR,b)
    EXPECT_EQ(Cell(model, 0, 0), "3");
    EXPECT_EQ(Cell(model, 1, 0), "4");
}

TEST_F(ColumnFilterTest, DistinctValuesNarrowByOtherColumnFilters)
{
    EventsTableModel model(m_events);
    model.SetColumnFilter(2, Set({"a"}));
    const auto d = model.DistinctColumnValues(1, 100); // level values among source=a rows
    ASSERT_EQ(d.values.size(), 2u);                    // WARN only occurs with source=b
    EXPECT_EQ(d.values[0].value, "ERROR");
    EXPECT_EQ(d.values[1].value, "INFO");
}

TEST_F(ColumnFilterTest, ComposesWithUpstreamFilter)
{
    EventsTableModel model(m_events);
    model.SetFilteredIndices({0, 1, 2}); // ids 1,2,3
    model.SetColumnFilter(1, Set({"INFO"}));
    ASSERT_EQ(model.rowCount(), 2);       // ids 1 and 3 (id 5's WARN irrelevant)
    EXPECT_EQ(Cell(model, 1, 0), "3");

    // A new upstream filter keeps the column filter in force.
    model.SetFilteredIndices({2, 3, 4});
    ASSERT_EQ(model.rowCount(), 1);
    EXPECT_EQ(Cell(model, 0, 0), "3");

    model.ClearFilter();
    EXPECT_EQ(model.rowCount(), 2); // still INFO only: ids 1 and 3
}

TEST_F(ColumnFilterTest, ClearRestoresPreviousRows)
{
    EventsTableModel model(m_events);
    model.SetColumnFilter(1, Set({"WARN"}));
    ASSERT_EQ(model.rowCount(), 1);
    model.ClearColumnFilter(1);
    EXPECT_EQ(model.rowCount(), 5);
    EXPECT_FALSE(model.IsFilteringActive());

    model.SetColumnFilter(1, Set({"WARN"}));
    model.ClearColumnFilters();
    EXPECT_EQ(model.rowCount(), 5);
    EXPECT_FALSE(model.HasAnyColumnFilter());
}

TEST_F(ColumnFilterTest, EmptyAllowedSetMatchesNothingAndSortKeepsItEmpty)
{
    EventsTableModel model(m_events);
    model.SetColumnFilter(1, {});
    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_TRUE(model.IsFilteringActive());
    model.sort(0, Qt::DescendingOrder); // must not resurrect the hidden rows
    EXPECT_EQ(model.rowCount(), 0);
}

TEST_F(ColumnFilterTest, ActiveSortSurvivesFilterChanges)
{
    EventsTableModel model(m_events);
    model.sort(0, Qt::DescendingOrder);
    model.SetColumnFilter(1, Set({"INFO", "ERROR"}));
    ASSERT_EQ(model.rowCount(), 4);
    EXPECT_EQ(Cell(model, 0, 0), "4"); // descending by id
    EXPECT_EQ(Cell(model, 3, 0), "1");
}

TEST_F(ColumnFilterTest, AppendedEventsAreTestedAgainstColumnFilters)
{
    EventsTableModel model(m_events);
    model.SetColumnFilter(1, Set({"ERROR"}));
    ASSERT_EQ(model.rowCount(), 2);

    Add(6, "ERROR", "c"); // e.g. follow-file mode appends
    Add(7, "INFO", "c");
    model.SyncWithContainer();
    ASSERT_EQ(model.rowCount(), 3);
    EXPECT_EQ(Cell(model, 2, 0), "6");
}

TEST_F(ColumnFilterTest, ViewDropsColumnFiltersWhenDataIsCleared)
{
    EventsTableView view(m_events);
    auto* model = view.findChild<EventsTableModel*>();
    ASSERT_NE(model, nullptr);
    model->SetColumnFilter(1, Set({"ERROR"}));
    ASSERT_TRUE(view.HasColumnFilters());

    m_events.Clear();
    view.RefreshView();
    EXPECT_FALSE(view.HasColumnFilters());
}

TEST_F(ColumnFilterTest, ColumnFiltersChangedSignalFires)
{
    EventsTableModel model(m_events);
    QSignalSpy spy(&model, &EventsTableModel::ColumnFiltersChanged);
    model.SetColumnFilter(1, Set({"INFO"}));
    model.ClearColumnFilter(1);
    model.ClearColumnFilter(1); // nothing to clear: no signal
    EXPECT_EQ(spy.count(), 2);
}

// ---------------------------------------------------------------------------
// Popup
// ---------------------------------------------------------------------------

class ColumnFilterPopupTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        d.values = {{"apple", 2}, {"banana", 1}, {"cherry", 3}};
    }
    ColumnDistinctValues d;
};

TEST_F(ColumnFilterPopupTest, StartsAllCheckedWithoutFilter)
{
    ColumnFilterPopup p("col", d, std::nullopt);
    EXPECT_EQ(p.CheckedValues().size(), 3);
    EXPECT_EQ(p.findChild<QCheckBox*>("columnFilterSelectAll")->checkState(), Qt::Checked);
}

TEST_F(ColumnFilterPopupTest, ReflectsExistingFilterAsPartialSelectAll)
{
    QSet<QString> allowed{"banana"};
    ColumnFilterPopup p("col", d, allowed);
    EXPECT_EQ(p.CheckedValues(), allowed);
    EXPECT_EQ(p.findChild<QCheckBox*>("columnFilterSelectAll")->checkState(), Qt::PartiallyChecked);
}

TEST_F(ColumnFilterPopupTest, SearchNarrowsSelectAllToVisibleRows)
{
    QSet<QString> none;
    ColumnFilterPopup p("col", d, none);
    p.findChild<QLineEdit*>("columnFilterSearch")->setText("an"); // banana only
    p.findChild<QCheckBox*>("columnFilterSelectAll")->click();
    EXPECT_EQ(p.CheckedValues(), (QSet<QString>{"banana"}));
}

TEST_F(ColumnFilterPopupTest, OkEmitsAppliedOrClearedWhenEverythingChecked)
{
    ColumnFilterPopup p("col", d, std::nullopt);
    QSignalSpy applied(&p, &ColumnFilterPopup::Applied);
    QSignalSpy cleared(&p, &ColumnFilterPopup::Cleared);

    p.findChild<QListWidget*>("columnFilterList")->item(0)->setCheckState(Qt::Unchecked);
    p.findChild<QPushButton*>("columnFilterOk")->click();
    ASSERT_EQ(applied.count(), 1);
    EXPECT_EQ(applied.takeFirst().at(0).value<QSet<QString>>(), (QSet<QString>{"banana", "cherry"}));

    ColumnFilterPopup all("col", d, std::nullopt);
    QSignalSpy cleared2(&all, &ColumnFilterPopup::Cleared);
    all.findChild<QPushButton*>("columnFilterOk")->click();
    EXPECT_EQ(cleared2.count(), 1);
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------

TEST(FilterHeaderViewTest, ClickOnIndicatorEmitsSignalAndOtherClicksDoNot)
{
    EnsureQApplication();
    QTableView table;
    QStandardItemModel model(2, 2);
    auto* header = new FilterHeaderView(Qt::Horizontal, &table);
    table.setHorizontalHeader(header);
    table.setModel(&model);
    table.resize(400, 200);
    header->resizeSection(0, 150);
    table.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&table));

    QSignalSpy spy(header, &FilterHeaderView::FilterIndicatorClicked);
    QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier,
                      header->IndicatorRect(0).center());
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.takeFirst().at(0).toInt(), 0);

    QTest::mouseClick(header->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(5, header->height() / 2));
    EXPECT_EQ(spy.count(), 0);
}

} // namespace ui::qt::test
