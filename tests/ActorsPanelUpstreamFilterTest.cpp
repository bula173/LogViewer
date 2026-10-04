// The actor filter works on top of the filter set elsewhere (type filter,
// time range): checking every actor again restores that filter instead of
// clearing all filters, the actor filter never shows events it hides, and
// restoring an empty unchecked set (filter profile) leaves it alone.
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

class ActorsPanelUpstreamFilterTest : public ::testing::Test
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

        std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
        const char* who[] = {"alice", "bob", "alice", "bob"};
        for (int i = 0; i < 4; ++i)
            batch.push_back({i + 1, {{"who", who[i]}}});
        m_events.AddEventBatch(std::move(batch));

        m_view  = std::make_unique<EventsTableView>(m_events);
        m_panel = std::make_unique<ActorsPanel>(m_events, m_view.get());
        ActorDefinition alice; alice.name = "Alice"; alice.pattern = "alice"; alice.field = "who";
        ActorDefinition bob;   bob.name   = "Bob";   bob.pattern   = "bob";   bob.field   = "who";
        m_panel->SetDefinitions({alice, bob});
    }
    void TearDown() override
    {
        m_panel.reset();
        m_view.reset();
        config::GetConfig().GetMutableColumns() = m_savedColumns;
        config::GetConfig().SetConfigFilePath(m_savedPath);
    }

    QTreeWidgetItem* Item(const char* name) const
    {
        auto* tree = m_panel->findChild<QTreeWidget*>();
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            if (tree->topLevelItem(i)->data(0, Qt::UserRole).toString() == name)
                return tree->topLevelItem(i);
        return nullptr;
    }
    const std::vector<unsigned long>* Base() const { return m_view->GetBaseFilteredIndices(); }

    QTemporaryDir                     m_dir;
    db::EventsContainer               m_events;
    std::unique_ptr<EventsTableView>  m_view;
    std::unique_ptr<ActorsPanel>      m_panel;
    std::vector<config::ColumnConfig> m_savedColumns;
    std::string                       m_savedPath;
};

TEST_F(ActorsPanelUpstreamFilterTest, CheckingAllActorsRestoresTheUpstreamFilter)
{
    const std::vector<unsigned long> upstream {0, 1, 2}; // e.g. the type filter
    m_view->SetFilteredEvents(upstream);
    m_panel->Refresh();

    Item("Bob")->setCheckState(0, Qt::Unchecked);
    QCoreApplication::processEvents();
    ASSERT_NE(Base(), nullptr);
    EXPECT_EQ(*Base(), (std::vector<unsigned long>{0, 2}));

    Item("Bob")->setCheckState(0, Qt::Checked);
    QCoreApplication::processEvents();
    ASSERT_NE(Base(), nullptr) << "the type filter must not be cleared";
    EXPECT_EQ(*Base(), upstream);
}

TEST_F(ActorsPanelUpstreamFilterTest, ActorFilterStaysWithinTheUpstreamFilter)
{
    // The tree was built from all events; then a time range is applied and a
    // filter profile unchecks Bob before the panel refreshed.
    m_view->SetFilteredEvents({0, 1});
    m_panel->RestoreUncheckedActors({ActorKey::Encode("Bob", "Bob")});
    QCoreApplication::processEvents();

    ASSERT_NE(Base(), nullptr);
    EXPECT_EQ(*Base(), (std::vector<unsigned long>{0}));
}

TEST_F(ActorsPanelUpstreamFilterTest, RestoringNoUncheckedActorsKeepsTheUpstreamFilter)
{
    m_view->SetFilteredEvents({0, 1}); // a profile's time range
    m_panel->RestoreUncheckedActors({}); // the same profile has no unchecked actors
    QCoreApplication::processEvents();

    ASSERT_NE(Base(), nullptr) << "the time range must not be cleared";
    EXPECT_EQ(*Base(), (std::vector<unsigned long>{0, 1}));
}

} // namespace ui::qt::test
