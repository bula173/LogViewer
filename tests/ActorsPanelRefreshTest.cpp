// The Actors panel skips the refresh caused by its own actor filter. That skip
// must not leak into a later load, and a filter must never be built from
// actor indices cached for a previous data set.
#include <gtest/gtest.h>
#include <QApplication>
#include <QTemporaryDir>
#include <QTreeWidget>

#include "qt/panels/ActorsPanel.hpp"
#include "qt/panels/ActorDefinition.hpp"
#include "qt/events/EventsTableView.hpp"
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

class ActorsPanelRefreshTest : public ::testing::Test
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
        cfg.GetMutableColumns() = {{"id", true, 50}, {"who", true, 80}};

        Load({"alice", "bob", "alice", "bob"}); // file A
        m_view  = std::make_unique<EventsTableView>(m_events);
        m_panel = std::make_unique<ActorsPanel>(m_events, m_view.get());
        ActorDefinition alice; alice.name = "Alice"; alice.pattern = "alice"; alice.field = "who";
        ActorDefinition bob;   bob.name   = "Bob";   bob.pattern   = "bob";   bob.field   = "who";
        m_panel->SetDefinitions({alice, bob});
        ASSERT_EQ(Tree()->topLevelItemCount(), 2);
    }
    void TearDown() override
    {
        m_panel.reset();
        m_view.reset();
        config::GetConfig().GetMutableColumns() = m_savedColumns;
        config::GetConfig().SetConfigFilePath(m_savedPath);
    }

    // Replaces the container contents the way the presenter does for a new file.
    void Load(std::vector<const char*> actors)
    {
        m_events.SuspendNotifications();
        m_events.Clear();
        std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
        for (std::size_t i = 0; i < actors.size(); ++i)
            batch.push_back({static_cast<int>(i + 1), {{"who", actors[i]}}});
        m_events.AddEventBatch(std::move(batch));
        m_events.ResumeNotifications();
        if (m_view)
        {
            m_view->ClearFilter();
            m_view->RefreshView();
        }
    }
    QTreeWidget* Tree() const { return m_panel->findChild<QTreeWidget*>(); }
    QTreeWidgetItem* Item(const char* name) const
    {
        for (int i = 0; i < Tree()->topLevelItemCount(); ++i)
            if (Tree()->topLevelItem(i)->data(0, Qt::UserRole).toString() == name)
                return Tree()->topLevelItem(i);
        return nullptr;
    }
    int Rows() const { return m_view->model()->rowCount(); }

    QTemporaryDir                     m_dir;
    db::EventsContainer               m_events;
    std::unique_ptr<EventsTableView>  m_view;
    std::unique_ptr<ActorsPanel>      m_panel;
    std::vector<config::ColumnConfig> m_savedColumns;
    std::string                       m_savedPath;
};

TEST_F(ActorsPanelRefreshTest, OwnFilterRefreshKeepsTheTree)
{
    Item("Bob")->setCheckState(0, Qt::Unchecked);
    QCoreApplication::processEvents();
    ASSERT_EQ(Rows(), 2);

    m_panel->Refresh(); // the refresh triggered by the panel's own model reset

    ASSERT_NE(Item("Bob"), nullptr);
    EXPECT_EQ(Item("Bob")->checkState(0), Qt::Unchecked);
    EXPECT_EQ(Rows(), 2);
}

TEST_F(ActorsPanelRefreshTest, SkippedRefreshDoesNotLeakIntoTheNextLoad)
{
    Item("Bob")->setCheckState(0, Qt::Unchecked);
    QCoreApplication::processEvents(); // filter applied, Actors tab never refreshed

    Load({"bob", "bob", "alice", "alice", "alice", "alice"}); // file B
    m_panel->Refresh();
    QCoreApplication::processEvents();

    // Toggle Alice off and on: the filter is rebuilt from the panel's cache.
    Item("Alice")->setCheckState(0, Qt::Unchecked);
    Item("Alice")->setCheckState(0, Qt::Checked);
    QCoreApplication::processEvents();
    EXPECT_EQ(Rows(), 4); // the four alice events of file B
}

TEST_F(ActorsPanelRefreshTest, FilterIsNotBuiltFromACacheOfThePreviousFile)
{
    Item("Bob")->setCheckState(0, Qt::Unchecked);
    QCoreApplication::processEvents();

    Load({"bob", "bob", "alice", "alice", "alice", "alice"}); // file B, panel not refreshed
    m_panel->RestoreUncheckedActors({ActorKey::Encode("Bob", "Bob")}); // e.g. a filter profile
    QCoreApplication::processEvents();

    EXPECT_EQ(Rows(), 4);
}

} // namespace ui::qt::test
