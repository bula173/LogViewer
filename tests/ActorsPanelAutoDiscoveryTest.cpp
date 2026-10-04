// Actors panel without actor definitions: the auto-discovered group lists
// every actor, counts an event under each sender and receiver, splits comma
// lists and ignores placeholders; the PlantUML export does the same.
#include <gtest/gtest.h>
#include <QApplication>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>

#include "qt/panels/ActorsPanel.hpp"
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

class ActorsPanelAutoDiscoveryTest : public ::testing::Test
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
        cfg.GetMutableColumns() = {{"from", true, 80}, {"to", true, 80}, {"msg", true, 80}};
        m_view  = std::make_unique<EventsTableView>(m_events);
        m_panel = std::make_unique<ActorsPanel>(m_events, m_view.get());
    }
    void TearDown() override
    {
        m_panel.reset();
        m_view.reset();
        config::GetConfig().GetMutableColumns() = m_savedColumns;
        config::GetConfig().SetConfigFilePath(m_savedPath);
    }

    void Load(const std::vector<std::pair<const char*, const char*>>& messages)
    {
        std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
        for (std::size_t i = 0; i < messages.size(); ++i)
            batch.push_back({static_cast<int>(i + 1), {{"from", messages[i].first},
                                                       {"to",   messages[i].second},
                                                       {"msg",  "hello"}}});
        m_events.AddEventBatch(std::move(batch));
        m_view->RefreshView();
        m_panel->Refresh();
    }

    QTreeWidget* Tree() const { return m_panel->findChild<QTreeWidget*>(); }
    // actor name → event count of the rows below the auto-discovered group
    std::map<std::string, int> Actors() const
    {
        std::map<std::string, int> out;
        for (int g = 0; g < Tree()->topLevelItemCount(); ++g)
        {
            QTreeWidgetItem* top = Tree()->topLevelItem(g);
            for (int c = 0; c < top->childCount(); ++c)
                out[top->child(c)->data(0, Qt::UserRole + 1).toString().toStdString()] =
                    top->child(c)->text(1).toInt();
        }
        return out;
    }

    QTemporaryDir                     m_dir;
    db::EventsContainer               m_events;
    std::unique_ptr<EventsTableView>  m_view;
    std::unique_ptr<ActorsPanel>      m_panel;
    std::vector<config::ColumnConfig> m_savedColumns;
    std::string                       m_savedPath;
};

TEST_F(ActorsPanelAutoDiscoveryTest, GroupListsEveryDiscoveredActor)
{
    Load({{"A", "B"}, {"B", "A"}, {"C", "A"}, {"A", "B"}});

    ASSERT_EQ(Tree()->topLevelItemCount(), 1);
    const auto actors = Actors();
    EXPECT_EQ(actors.size(), 3u);
    EXPECT_EQ(actors.count("A"), 1u);
    EXPECT_EQ(actors.count("B"), 1u);
    EXPECT_EQ(actors.count("C"), 1u);
}

TEST_F(ActorsPanelAutoDiscoveryTest, CountsSendersAndReceiversSplitsListsSkipsPlaceholders)
{
    Load({{"A", "internal"}, {"B", "c-west,c-east"}, {"A", "B"}, {"A", "internal"}});

    const std::map<std::string, int> expected {
        {"A", 3}, {"B", 2}, {"c-west", 1}, {"c-east", 1}};
    EXPECT_EQ(Actors(), expected);
}

TEST_F(ActorsPanelAutoDiscoveryTest, PlantUmlSplitsListsAndSkipsPlaceholders)
{
    Load({{"A", "internal"}, {"B", "c-west,c-east"}, {"A", "B"}, {"A", "internal"}});

    QPushButton* seqButton = nullptr;
    for (auto* b : m_panel->findChildren<QPushButton*>())
        if (b->text().startsWith("Sequence Diagram")) seqButton = b;
    ASSERT_NE(seqButton, nullptr);
    seqButton->click();

    auto* output = m_panel->findChild<QPlainTextEdit*>();
    ASSERT_NE(output, nullptr);
    const QString puml = output->toPlainText();
    EXPECT_TRUE(puml.contains("\"B\" ->> \"c-west\"")) << puml.toStdString();
    EXPECT_TRUE(puml.contains("\"B\" ->> \"c-east\"")) << puml.toStdString();
    EXPECT_TRUE(puml.contains("\"A\" ->> \"B\"")) << puml.toStdString();
    EXPECT_FALSE(puml.contains("internal")) << puml.toStdString();
    EXPECT_FALSE(puml.contains("c-west,c-east")) << puml.toStdString();
    output->window()->close();
}

} // namespace ui::qt::test
