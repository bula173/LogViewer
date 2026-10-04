// Quick filters of the events table context menu: show only / exclude a cell
// value, a time window around an event and the conversation of two actors.
#include <gtest/gtest.h>
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QTemporaryDir>

#include <cstdlib>

#include "qt/events/EventsTableModel.hpp"
#include "qt/events/EventsTableView.hpp"
#include "qt/events/QuickFilters.hpp"
#include "EventsContainer.hpp"
#include "Config.hpp"

namespace ui::qt::test {

namespace qf = quick_filters;

namespace {
void EnsureQApplication()
{
    if (QApplication::instance()) return;
    static int argc = 1;
    static char argv0[] = "tests";
    static char* argv[] = {argv0};
    static QApplication app(argc, argv);
}

QSet<QString> Set(std::initializer_list<const char*> v)
{
    QSet<QString> s;
    for (auto* x : v) s.insert(x);
    return s;
}

/// Every action of @p menu and its submenus.
std::vector<QAction*> AllActions(const QMenu& menu)
{
    std::vector<QAction*> out;
    for (QAction* a : menu.actions())
    {
        out.push_back(a);
        if (a->menu())
            for (QAction* sub : AllActions(*a->menu()))
                out.push_back(sub);
    }
    return out;
}

QAction* FindAction(const QMenu& menu, const QString& text)
{
    for (QAction* a : AllActions(menu))
        if (a->text() == text) return a;
    return nullptr;
}
} // namespace

// ── Pure combining rules ────────────────────────────────────────────────────

TEST(QuickFilterCombineTest, ShowOnlyOnAnUnfilteredColumnShowsJustThatValue)
{
    EXPECT_EQ(qf::ShowOnly(std::nullopt, Set({"a"})), (qf::ColumnFilterSpec{Set({"a"}), false}));
}

TEST(QuickFilterCombineTest, ShowOnlyNeverWidensAnInclusionFilter)
{
    const qf::ColumnFilterSpec current{Set({"a", "b"}), false};
    EXPECT_EQ(qf::ShowOnly(current, Set({"a"})), (qf::ColumnFilterSpec{Set({"a"}), false}));
    // A value the filter hides stays hidden.
    EXPECT_EQ(qf::ShowOnly(current, Set({"c"})), (qf::ColumnFilterSpec{{}, false}));
}

TEST(QuickFilterCombineTest, ShowOnlyOnAnExclusionFilterKeepsTheExcludedValuesOut)
{
    const qf::ColumnFilterSpec current{Set({"c"}), true};
    EXPECT_EQ(qf::ShowOnly(current, Set({"a", "c"})), (qf::ColumnFilterSpec{Set({"a"}), false}));
}

TEST(QuickFilterCombineTest, ExcludeAddsToAnExclusionAndRemovesFromAnInclusion)
{
    EXPECT_EQ(qf::Exclude(std::nullopt, "a"), (qf::ColumnFilterSpec{Set({"a"}), true}));
    EXPECT_EQ(qf::Exclude(qf::ColumnFilterSpec{Set({"a"}), true}, "b"),
              (qf::ColumnFilterSpec{Set({"a", "b"}), true}));
    EXPECT_EQ(qf::Exclude(qf::ColumnFilterSpec{Set({"a", "b"}), false}, "a"),
              (qf::ColumnFilterSpec{Set({"b"}), false}));
}

TEST(QuickFilterMenuTextTest, LongValuesAreElidedAndAmpersandsKept)
{
    EXPECT_EQ(qf::MenuText("short"), "short");
    const QString longValue(100, 'x');
    const QString elided = qf::MenuText(longValue, 10);
    EXPECT_EQ(elided.size(), 10);
    EXPECT_TRUE(elided.endsWith(QChar(0x2026)));
    EXPECT_EQ(qf::MenuText("R&D"), "R&&D");
    EXPECT_EQ(qf::MenuText("two\nlines"), "two lines");
}

// ── Conversation helpers ────────────────────────────────────────────────────

TEST(QuickFilterConversationTest, FindsSenderAndReceiverColumns)
{
    const auto cols = qf::FindActorColumns({"timestamp", "protocol", "Source", "Destination"});
    ASSERT_TRUE(cols);
    EXPECT_EQ(cols->sender, 2);
    EXPECT_EQ(cols->receiver, 3); // not "protocol", which merely contains "to"

    const auto fallback = qf::FindActorColumns({"msg_from", "msg_to"});
    ASSERT_TRUE(fallback);
    EXPECT_EQ(fallback->sender, 0);
    EXPECT_EQ(fallback->receiver, 1);

    EXPECT_FALSE(qf::FindActorColumns({"timestamp", "source", "message"}));
    // An empty name (the dynamic merge-source column) is never an actor column.
    EXPECT_FALSE(qf::FindActorColumns({"", "destination"}));
}

TEST(QuickFilterConversationTest, PairsFanOutListsAndDropPlaceholdersAndSelf)
{
    using Pairs = std::vector<std::pair<QString, QString>>;
    EXPECT_EQ(qf::ConversationPairs("a", "b"), (Pairs{{"a", "b"}}));
    EXPECT_EQ(qf::ConversationPairs("c", "a, b"), (Pairs{{"c", "a"}, {"c", "b"}}));
    EXPECT_EQ(qf::ConversationPairs("a, b", "b, internal"), (Pairs{{"a", "b"}}));
    EXPECT_EQ(qf::ConversationPairs("a, b", "a, b"), (Pairs{{"a", "b"}})); // b→a is the same pair
    EXPECT_TRUE(qf::ConversationPairs("a", "none").empty());
    EXPECT_TRUE(qf::ConversationPairs("a", "a").empty());
}

TEST(QuickFilterConversationTest, CellsNamingEitherActor)
{
    EXPECT_EQ(qf::CellsNaming(Set({"a", "b", "c", "b, c", "internal", "c,a"}), "a", "b"),
              Set({"a", "b", "b, c", "c,a"}));
}

// ── Model and view ──────────────────────────────────────────────────────────

// Columns: 0=timestamp 1=source 2=destination 3=message
class QuickFiltersTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        auto& cfg = config::GetConfig();
        m_savedColumns = cfg.GetColumns();
        m_savedPath    = cfg.GetConfigFilePath();
        ASSERT_TRUE(m_dir.isValid());
        cfg.SetConfigFilePath((m_dir.path() + "/config.json").toStdString());
        cfg.GetMutableColumns() = {{"timestamp", true, 80}, {"source", true, 60},
                                   {"destination", true, 60}, {"message", true, 80}};

        Add(1, "2026-01-01 10:00:00.000", "a", "b", "REQ");        // 0
        Add(2, "2026-01-01 10:00:01.500", "b", "a", "RSP");        // 1
        Add(3, "2026-01-01T10:00:02.400", "a", "b, c", "BCAST");   // 2
        Add(4, "2026-01-01 10:00:02.600", "c", "internal", "LOC"); // 3
        Add(5, "2026-01-01 10:00:05", "c", "a", "X");              // 4
        Add(6, "", "a", "b", "NO_TS");                             // 5
        Add(7, "2026-01-01 10:00:10", "a", "a", "SELF");           // 6
    }
    void TearDown() override
    {
        m_view.reset();
        config::GetConfig().GetMutableColumns() = m_savedColumns;
        config::GetConfig().SetConfigFilePath(m_savedPath);
    }

    void Add(int id, const char* ts, const char* src, const char* dst, const char* msg)
    {
        db::LogEvent::EventItems items{{"source", src}, {"destination", dst}, {"message", msg}};
        if (*ts) items.push_back({"timestamp", ts});
        m_events.AddEvent(db::LogEvent(id, std::move(items)));
    }

    EventsTableView& View()
    {
        if (!m_view) m_view = std::make_unique<EventsTableView>(m_events);
        return *m_view;
    }
    /// Messages of the rows the view shows, in order.
    QStringList Messages()
    {
        QStringList out;
        auto* model = View().model();
        for (int r = 0; r < model->rowCount(); ++r)
            out << model->data(model->index(r, 3), Qt::DisplayRole).toString();
        return out;
    }
    std::vector<unsigned long> Base()
    {
        const auto* base = View().GetBaseFilteredIndices();
        return base ? *base : std::vector<unsigned long>{};
    }

    QTemporaryDir                     m_dir;
    db::EventsContainer               m_events;
    std::unique_ptr<EventsTableView>  m_view;
    std::vector<config::ColumnConfig> m_savedColumns;
    std::string                       m_savedPath;
};

TEST_F(QuickFiltersTest, ShowOnlyAndExcludeCombineWithTheColumnFilter)
{
    EventsTableModel model(m_events);

    qf::ApplyShowOnly(model, 1, {"a"});
    EXPECT_EQ(model.rowCount(), 4);
    EXPECT_FALSE(model.IsColumnFilterExclusion(1));

    // Excluding from an inclusion narrows it further.
    qf::ApplyExclude(model, 1, "a");
    EXPECT_EQ(model.rowCount(), 0);

    model.ClearColumnFilters();
    qf::ApplyExclude(model, 1, "a");
    qf::ApplyExclude(model, 1, "b");
    EXPECT_TRUE(model.IsColumnFilterExclusion(1));
    EXPECT_EQ(model.ColumnFilterValues(1), Set({"a", "b"}));
    EXPECT_EQ(model.rowCount(), 2); // c, c

    // Show only on an excluded column turns it into an inclusion of the value.
    qf::ApplyShowOnly(model, 1, {"c"});
    EXPECT_FALSE(model.IsColumnFilterExclusion(1));
    EXPECT_EQ(model.ColumnFilterValues(1), Set({"c"}));
    EXPECT_EQ(model.rowCount(), 2);
}

TEST_F(QuickFiltersTest, MenuOffersTheCellValueAndItsActionsApplyColumnFilters)
{
    auto& view = View();
    QMenu menu;
    view.AddQuickFilterActions(menu, view.model()->index(0, 1));

    ASSERT_NE(FindAction(menu, "Show Only source = \"a\""), nullptr);
    QAction* exclude = FindAction(menu, "Exclude source = \"a\"");
    ASSERT_NE(exclude, nullptr);

    exclude->trigger();
    EXPECT_EQ(Messages(), (QStringList{"RSP", "LOC", "X"}));
    EXPECT_TRUE(view.HasColumnFilters());

    // Undo: the existing "Clear All Column Filters".
    view.ClearColumnFilters();
    EXPECT_EQ(view.model()->rowCount(), 7);
}

TEST_F(QuickFiltersTest, TimeWindowIsInclusiveAtMillisecondPrecision)
{
    auto& view = View();
    ASSERT_TRUE(view.ShowTimeWindow(1, 1)); // 10:00:01.500 ± 1 s
    EXPECT_EQ(Messages(), (QStringList{"RSP", "BCAST"})); // 02.400 in, 02.600 and 00.000 out
    EXPECT_TRUE(view.HasTimeWindow());

    ASSERT_TRUE(view.ShowTimeWindow(0, 5)); // exactly 10:00:05 is on the boundary
    EXPECT_EQ(Messages(), (QStringList{"REQ", "RSP", "BCAST", "LOC", "X"}));
}

TEST_F(QuickFiltersTest, TimeWindowNeedsAParseableTimestamp)
{
    auto& view = View();
    EXPECT_FALSE(view.ShowTimeWindow(5, 10));
    EXPECT_FALSE(view.HasTimeWindow());
    EXPECT_EQ(view.GetBaseFilteredIndices(), nullptr);

    QMenu menu;
    view.AddQuickFilterActions(menu, view.model()->index(5, 3));
    QAction* window = FindAction(menu, "Show Events Around This One");
    ASSERT_NE(window, nullptr);
    EXPECT_FALSE(window->isEnabled());

    QMenu timed;
    view.AddQuickFilterActions(timed, view.model()->index(0, 3));
    ASSERT_NE(FindAction(timed, "Show Events Around This One"), nullptr);
    EXPECT_TRUE(FindAction(timed, "Show Events Around This One")->isEnabled());
    QAction* preset = FindAction(timed, QString::fromUtf8("±2 s"));
    ASSERT_NE(preset, nullptr);
    preset->trigger();
    EXPECT_EQ(Messages(), (QStringList{"REQ", "RSP"}));
}

TEST_F(QuickFiltersTest, FractionalSecondsBeyondMillisecondsParse)
{
    db::LogEvent ev(1, {{"timestamp", "2026-10-04T01:55:06.402978+00:00"}});
    const auto anchor = qf::FindTimeAnchor(ev);
    ASSERT_TRUE(anchor);
    EXPECT_EQ(anchor->field, "timestamp");
    const QDateTime expected =
        QDateTime::fromString("2026-10-04T01:55:06.402Z", Qt::ISODateWithMs);
    EXPECT_LE(std::abs(expected.msecsTo(anchor->time)), 1);

    EXPECT_FALSE(qf::FindTimeAnchor(db::LogEvent(2, {{"timestamp", "not a time"}})));
}

TEST_F(QuickFiltersTest, TimeWindowNarrowsTheUpstreamFilterAndClearRestoresIt)
{
    auto& view = View();
    view.SetFilteredEvents({0, 2, 4, 6}); // e.g. a type filter

    ASSERT_TRUE(view.ShowTimeWindow(0, 5));
    EXPECT_EQ(Base(), (std::vector<unsigned long>{0, 2, 4}));

    view.ClearTimeWindow();
    EXPECT_FALSE(view.HasTimeWindow());
    EXPECT_EQ(Base(), (std::vector<unsigned long>{0, 2, 4, 6}));
}

TEST_F(QuickFiltersTest, ANewTimeWindowReplacesThePreviousOne)
{
    auto& view = View();
    ASSERT_TRUE(view.ShowTimeWindow(1, 1));
    ASSERT_TRUE(view.ShowTimeWindow(6, 5)); // 10:00:10 ± 5 s, not inside the first window
    EXPECT_EQ(Messages(), (QStringList{"X", "SELF"}));

    view.ClearTimeWindow();
    EXPECT_EQ(view.GetBaseFilteredIndices(), nullptr);
    EXPECT_EQ(view.model()->rowCount(), 7);
}

TEST_F(QuickFiltersTest, ClearTimeWindowLeavesAFilterSetByOthersAlone)
{
    auto& view = View();
    ASSERT_TRUE(view.ShowTimeWindow(1, 1));
    view.SetFilteredEvents({3});
    EXPECT_FALSE(view.HasTimeWindow());
    view.ClearTimeWindow();
    EXPECT_EQ(Base(), (std::vector<unsigned long>{3}));
}

TEST_F(QuickFiltersTest, ConversationShowsBothDirectionsIncludingCommaLists)
{
    auto& view = View();
    QMenu menu;
    view.AddQuickFilterActions(menu, view.model()->index(0, 0));
    QAction* conversation = FindAction(menu, QString::fromUtf8("Conversation a ↔ b"));
    ASSERT_NE(conversation, nullptr);

    conversation->trigger();
    // a→b, b→a, a→"b, c" and the untimed a→b; c→internal and c→a are out.
    // A message of one of them to itself also passes (column filters are per column).
    EXPECT_EQ(Messages(), (QStringList{"REQ", "RSP", "BCAST", "NO_TS", "SELF"}));
}

TEST_F(QuickFiltersTest, ConversationDoesNotWidenAnExistingFilter)
{
    EventsTableModel model(m_events);
    qf::ApplyExclude(model, 1, "b");
    qf::ApplyConversation(model, {1, 2}, "a", "b");
    EXPECT_EQ(model.ColumnFilterValues(1), Set({"a"}));
    EXPECT_EQ(model.rowCount(), 4); // REQ, BCAST, NO_TS, SELF
}

TEST_F(QuickFiltersTest, NoConversationWithoutActorColumnsOrWithAPlaceholder)
{
    auto& view = View();
    QMenu placeholder;
    view.AddQuickFilterActions(placeholder, view.model()->index(3, 0)); // c → internal
    for (QAction* a : AllActions(placeholder))
        EXPECT_FALSE(a->text().startsWith("Conversation")) << a->text().toStdString();

    m_view.reset();
    config::GetConfig().GetMutableColumns() = {{"timestamp", true, 80}, {"message", true, 80}};
    auto& plain = View();
    QMenu menu;
    plain.AddQuickFilterActions(menu, plain.model()->index(0, 0));
    for (QAction* a : AllActions(menu))
        EXPECT_FALSE(a->text().startsWith("Conversation")) << a->text().toStdString();
}

} // namespace ui::qt::test
