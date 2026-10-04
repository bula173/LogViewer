// Regression tests for the events-table review fixes (v1.13.3): stale filter
// state after a reload, no implicit startup sort, column-state bookkeeping,
// selection / search state across model resets and the column filter popup.
#include <gtest/gtest.h>
#include <QApplication>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

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
class EventsTableStateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        auto& cfg = config::GetConfig();
        m_savedColumns = cfg.GetColumns();
        m_savedOrder   = cfg.columnOrder;
        m_savedWidths  = cfg.columnWidths;
        m_savedPath    = cfg.GetConfigFilePath();
        // The view saves column widths (and the config file) on every resize.
        ASSERT_TRUE(m_dir.isValid());
        cfg.SetConfigFilePath((m_dir.path() + "/config.json").toStdString());
        cfg.columnOrder.clear();
        cfg.columnWidths.clear();
        SetColumns({{"id", true, 50}, {"level", true, 80}, {"source", true, 80}});
        Add(1, "INFO", "a"); Add(2, "ERROR", "a"); Add(3, "INFO", "b");
        Add(4, "ERROR", "b"); Add(5, "WARN", "b");
    }
    void TearDown() override
    {
        auto& cfg = config::GetConfig();
        cfg.GetMutableColumns() = m_savedColumns;
        cfg.columnOrder  = m_savedOrder;
        cfg.columnWidths = m_savedWidths;
        cfg.SetConfigFilePath(m_savedPath);
    }

    static void SetColumns(std::vector<config::ColumnConfig> columns)
    {
        config::GetConfig().GetMutableColumns() = std::move(columns);
    }
    void Add(int id, const char* lvl, const char* src)
    {
        m_events.AddEvent(db::LogEvent(id, {{"level", lvl}, {"source", src}}));
    }
    static QString Cell(const QAbstractItemModel& m, int r, int c)
    { return m.data(m.index(r, c), Qt::DisplayRole).toString(); }
    static QStringList Column(const QAbstractItemModel& m, int c)
    {
        QStringList out;
        for (int r = 0; r < m.rowCount(); ++r)
            out << Cell(m, r, c);
        return out;
    }
    static QSet<QString> Set(std::initializer_list<const char*> v)
    { QSet<QString> s; for (auto* x : v) s.insert(x); return s; }
    static EventsTableModel* ModelOf(EventsTableView& view)
    { return static_cast<EventsTableModel*>(view.model()); }

    QTemporaryDir                     m_dir;
    db::EventsContainer               m_events;
    std::vector<config::ColumnConfig> m_savedColumns;
    std::vector<std::string>          m_savedOrder;
    std::map<std::string, int>        m_savedWidths;
    std::string                       m_savedPath;
};

// ── Stale state after the container is replaced ─────────────────────────────

TEST_F(EventsTableStateTest, StaleUpstreamFilterIsDroppedWhenTheContainerShrinks)
{
    EventsTableModel model(m_events);
    model.SetColumnFilterExcluding(1, Set({"NOPE"}));
    model.SetFilteredIndices({3, 4});
    ASSERT_EQ(model.rowCount(), 2);

    m_events.Clear();
    Add(11, "INFO", "x"); Add(12, "INFO", "x"); Add(13, "INFO", "x");
    model.SyncWithContainer();

    EXPECT_EQ(model.rowCount(), 3); // used to show zero rows
}

TEST_F(EventsTableStateTest, UpstreamFilterOfAnOlderDataSetIsDroppedEvenWhenInRange)
{
    EventsTableModel model(m_events);
    model.SetFilteredIndices({3, 4});

    m_events.Clear();
    for (int id = 11; id <= 16; ++id)
        Add(id, "INFO", "x");
    model.SyncWithContainer();

    EXPECT_EQ(model.rowCount(), 6);
}

TEST_F(EventsTableStateTest, SortedListIsRebuiltWhenTheContainerIsReplaced)
{
    EventsTableModel model(m_events);
    model.sort(0, Qt::DescendingOrder);

    m_events.Clear(); // same size again, but a different order of ids
    for (int id = 15; id >= 11; --id)
        Add(id, "INFO", "x");
    model.SyncWithContainer();

    EXPECT_EQ(Column(model, 0), (QStringList{"15", "14", "13", "12", "11"}));
}

TEST_F(EventsTableStateTest, ContainerGenerationChangesOnClearAndMergeOnly)
{
    const auto start = m_events.Generation();
    Add(6, "INFO", "a");
    EXPECT_EQ(m_events.Generation(), start);

    db::EventsContainer other;
    other.AddEvent(db::LogEvent(1, {{"timestamp", "1"}}));
    m_events.MergeEvents(other, "a", "b");
    const auto afterMerge = m_events.Generation();
    EXPECT_NE(afterMerge, start);

    m_events.Clear();
    EXPECT_NE(m_events.Generation(), afterMerge);
}

// ── Sort / column state ─────────────────────────────────────────────────────

TEST_F(EventsTableStateTest, SortDroppedWithItsRemovedColumnRestoresNaturalOrder)
{
    EventsTableModel model(m_events);
    model.sort(1, Qt::AscendingOrder); // by level

    SetColumns({{"id", true, 50}, {"level", false, 80}, {"source", true, 80}});
    model.RefreshColumns(); // hidden: the sort is kept
    SetColumns({{"id", true, 50}, {"source", true, 80}});
    model.RefreshColumns(); // removed: the sort is dropped

    EXPECT_EQ(Column(model, 0), (QStringList{"1", "2", "3", "4", "5"}));
    Add(6, "INFO", "a");
    model.SyncWithContainer();
    EXPECT_EQ(model.rowCount(), 6);
}

TEST_F(EventsTableStateTest, FilterOfAHiddenColumnDoesNotCountAsActive)
{
    EventsTableView view(m_events);
    ModelOf(view)->SetColumnFilter(1, Set({"ERROR"}));
    ASSERT_TRUE(view.HasColumnFilters());

    SetColumns({{"id", true, 50}, {"level", false, 80}, {"source", true, 80}});
    view.RefreshColumns();

    EXPECT_FALSE(view.HasColumnFilters());       // no "Clear All Column Filters"
    EXPECT_FALSE(view.IsFilterActive());          // "no filter" fast paths stay usable
    EXPECT_TRUE(ModelOf(view)->HasAnyColumnFilter()); // still kept for later
}

TEST_F(EventsTableStateTest, ViewDoesNotSortImplicitlyAtStartup)
{
    EventsTableView view(m_events);
    view.show();

    EXPECT_EQ(view.GetFilteredIndices(), nullptr);
    EXPECT_FALSE(view.IsFilterActive());
    EXPECT_EQ(view.horizontalHeader()->sortIndicatorSection(), -1);
    EXPECT_EQ(Cell(*view.model(), 0, 0), "1");
}

TEST_F(EventsTableStateTest, SortIndicatorFollowsItsColumnAfterColumnChanges)
{
    EventsTableView view(m_events);
    view.show();
    view.sortByColumn(1, Qt::AscendingOrder); // level
    ASSERT_EQ(view.horizontalHeader()->sortIndicatorSection(), 1);

    SetColumns({{"level", true, 80}, {"id", true, 50}, {"source", true, 80}});
    view.RefreshColumns();
    EXPECT_EQ(view.horizontalHeader()->sortIndicatorSection(), 0);
    EXPECT_EQ(view.horizontalHeader()->sortIndicatorOrder(), Qt::AscendingOrder);

    SetColumns({{"level", false, 80}, {"id", true, 50}, {"source", true, 80}});
    view.RefreshColumns();
    EXPECT_EQ(view.horizontalHeader()->sortIndicatorSection(), -1);
}

TEST_F(EventsTableStateTest, ColumnWidthsAreSavedAndRestoredByColumnName)
{
    SetColumns({{"id", true, 50}, {"level", false, 80}, {"source", true, 80}, {"msg", true, 90}});
    config::GetConfig().columnWidths = {{"source", 150}, {"level", 77}};

    EventsTableView view(m_events); // model columns: 0=id 1=source 2=msg
    EXPECT_EQ(view.horizontalHeader()->sectionSize(1), 150);

    view.horizontalHeader()->resizeSection(1, 222);
    const auto& widths = config::GetConfig().columnWidths;
    ASSERT_TRUE(widths.count("source"));
    EXPECT_EQ(widths.at("source"), 222);
    ASSERT_TRUE(widths.count("level"));
    EXPECT_EQ(widths.at("level"), 77); // hidden column keeps its width
}

TEST_F(EventsTableStateTest, ColumnOrderIsSavedAndRestoredByColumnName)
{
    SetColumns({{"id", true, 50}, {"level", false, 80}, {"source", true, 80}, {"msg", true, 90}});
    config::GetConfig().columnOrder = {"msg", "source", "id"};

    {
        EventsTableView view(m_events); // model columns: 0=id 1=source 2=msg
        QHeaderView* header = view.horizontalHeader();
        EXPECT_EQ(header->visualIndex(2), 0);
        EXPECT_EQ(header->visualIndex(1), 1);
        EXPECT_EQ(header->visualIndex(0), 2);
    }

    config::GetConfig().columnOrder.clear();
    EventsTableView view(m_events);
    view.horizontalHeader()->moveSection(2, 0); // drag msg to the front
    EXPECT_EQ(config::GetConfig().columnOrder,
              (std::vector<std::string>{"msg", "id", "source"}));
}

// ── Selection and search across model resets ────────────────────────────────

TEST_F(EventsTableStateTest, SelectionFollowsItsEventThroughAFilterChange)
{
    EventsTableView view(m_events);
    view.show();
    view.ScrollToActualRow(3, false);
    ASSERT_EQ(view.CurrentActualRow(), 3);
    QSignalSpy current(&view, &EventsTableView::CurrentActualRowChanged);

    view.SetFilteredEvents({1, 3, 4});
    EXPECT_EQ(view.CurrentActualRow(), 3);
    const auto selected = view.selectionModel()->selectedRows();
    ASSERT_EQ(selected.size(), 1);
    EXPECT_EQ(view.ResolveToActualIndex(selected.first().row()), 3);
    EXPECT_EQ(current.count(), 0); // same event: no Item Details update

    view.SetFilteredEvents({0, 1});
    EXPECT_EQ(view.CurrentActualRow(), -1);
    ASSERT_GE(current.count(), 1);
    EXPECT_EQ(current.last().at(0).toInt(), -1); // Item Details must drop the hidden event
}

TEST_F(EventsTableStateTest, SearchCounterFollowsAFilterChange)
{
    EventsTableView view(m_events);
    view.show();
    QSignalSpy info(&view, &EventsTableView::MatchInfoChanged);
    view.SetSearchTerm("ERROR", false); // rows of events 2 and 4 (actual 1 and 3)
    view.NavigateToNextMatch();
    ASSERT_EQ(info.last().at(0).toInt(), 2);
    ASSERT_EQ(info.last().at(1).toInt(), 2);

    view.SetFilteredEvents({3, 4}); // only the current match (actual 3) stays
    EXPECT_EQ(info.last().at(0).toInt(), 1);
    EXPECT_EQ(info.last().at(1).toInt(), 1);
}

// ── Column filter popup: OK must not drop or widen what it cannot list ─────

class EventsTablePopupTest : public EventsTableStateTest
{
protected:
    // Opens the level column's popup, presses OK unchanged, then removes the
    // source filter that narrowed the popup's value list.
    static void PressOkOnLevelPopup(EventsTableView& view)
    {
        auto* header = view.findChild<FilterHeaderView*>();
        ASSERT_NE(header, nullptr);
        emit header->FilterIndicatorClicked(1);
        auto* popup = view.findChild<ColumnFilterPopup*>();
        ASSERT_NE(popup, nullptr);
        popup->findChild<QPushButton*>("columnFilterOk")->click();
        delete popup;
        ModelOf(view)->ClearColumnFilter(2);
    }
};

TEST_F(EventsTablePopupTest, OkKeepsAllowedValuesTheListCouldNotShow)
{
    EventsTableView view(m_events);
    ModelOf(view)->SetColumnFilter(1, Set({"INFO", "WARN"}));
    ModelOf(view)->SetColumnFilter(2, Set({"a"})); // hides the WARN row from the level list

    PressOkOnLevelPopup(view);
    EXPECT_EQ(Column(*view.model(), 1), (QStringList{"INFO", "INFO", "WARN"}));
}

TEST_F(EventsTablePopupTest, OkWithEverythingCheckedKeepsRestrictionsOnUnlistedValues)
{
    EventsTableView view(m_events);
    ModelOf(view)->SetColumnFilter(1, Set({"INFO", "ERROR"}));
    ModelOf(view)->SetColumnFilter(2, Set({"a"}));

    PressOkOnLevelPopup(view);
    EXPECT_EQ(view.model()->rowCount(), 4); // WARN stays hidden
}

TEST_F(EventsTablePopupTest, OkKeepsExcludedValuesTheListCouldNotShow)
{
    EventsTableView view(m_events);
    ModelOf(view)->SetColumnFilterExcluding(1, Set({"WARN"}));
    ModelOf(view)->SetColumnFilter(2, Set({"a"}));

    PressOkOnLevelPopup(view);
    EXPECT_EQ(view.model()->rowCount(), 4);
}

TEST_F(EventsTablePopupTest, OkWithEverythingCheckedClearsAnUnnarrowedFilter)
{
    EventsTableView view(m_events);
    ModelOf(view)->SetColumnFilter(1, Set({"INFO"}));

    auto* header = view.findChild<FilterHeaderView*>();
    emit header->FilterIndicatorClicked(1);
    auto* popup = view.findChild<ColumnFilterPopup*>();
    ASSERT_NE(popup, nullptr);
    auto* list = popup->findChild<QListWidget*>("columnFilterList");
    for (int i = 0; i < list->count(); ++i)
        list->item(i)->setCheckState(Qt::Checked);
    popup->findChild<QPushButton*>("columnFilterOk")->click();
    delete popup;

    EXPECT_FALSE(ModelOf(view)->HasColumnFilter(1));
    EXPECT_EQ(view.model()->rowCount(), 5);
}

} // namespace ui::qt::test
