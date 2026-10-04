// Statistics: the Top Values and Field Statistics tables read the visible
// rows of a filtered view, not the rows numbered like container indices.
#include <gtest/gtest.h>
#include <QApplication>
#include <QTableWidget>
#include <QTemporaryDir>

#include "qt/panels/StatsSummaryPanel.hpp"
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

class StatsSummaryPanelFilterTest : public ::testing::Test
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
        cfg.GetMutableColumns() = {{"kind", true, 80}};

        std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
        const char* kinds[] = {"other", "other", "picked", "picked"};
        for (int i = 0; i < 4; ++i)
            batch.push_back({i + 1, {{"kind", kinds[i]}}});
        m_events.AddEventBatch(std::move(batch));

        m_view  = std::make_unique<EventsTableView>(m_events);
        m_panel = std::make_unique<StatsSummaryPanel>(m_events, m_view.get());
    }
    void TearDown() override
    {
        m_panel.reset();
        m_view.reset();
        config::GetConfig().GetMutableColumns() = m_savedColumns;
        config::GetConfig().SetConfigFilePath(m_savedPath);
    }

    // The table whose header column @p col reads @p header.
    QTableWidget* Table(int col, const char* header) const
    {
        for (auto* t : m_panel->findChildren<QTableWidget*>())
            if (t->columnCount() > col && t->horizontalHeaderItem(col)
                && t->horizontalHeaderItem(col)->text() == header)
                return t;
        return nullptr;
    }

    QTemporaryDir                     m_dir;
    db::EventsContainer               m_events;
    std::unique_ptr<EventsTableView>  m_view;
    std::unique_ptr<StatsSummaryPanel> m_panel;
    std::vector<config::ColumnConfig> m_savedColumns;
    std::string                       m_savedPath;
};

TEST_F(StatsSummaryPanelFilterTest, TopValuesAndFieldStatsUseTheFilteredRows)
{
    m_view->SetFilteredEvents({2, 3});
    m_panel->Refresh();

    auto* topN = Table(1, "Value");
    ASSERT_NE(topN, nullptr);
    ASSERT_EQ(topN->rowCount(), 1);
    EXPECT_EQ(topN->item(0, 1)->text(), "picked");
    EXPECT_EQ(topN->item(0, 2)->text(), "2");

    auto* fields = Table(2, "Fill %");
    ASSERT_NE(fields, nullptr);
    ASSERT_GE(fields->rowCount(), 1);
    EXPECT_EQ(fields->item(0, 1)->text(), "1");      // one distinct value
    EXPECT_EQ(fields->item(0, 2)->text(), "100.0%"); // every visible row filled
}

} // namespace ui::qt::test
