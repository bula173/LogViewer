// Pattern analysis: clicking a template filters the events view (and the
// refresh that follows keeps the template list); a sequence of exactly n
// events yields one n-gram.
#include <gtest/gtest.h>
#include <QApplication>
#include <QTableWidget>
#include <QTemporaryDir>

#include "qt/panels/PatternAnalysisPanel.hpp"
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

class PatternAnalysisPanelTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        auto& cfg = config::GetConfig();
        m_savedColumns   = cfg.GetColumns();
        m_savedPath      = cfg.GetConfigFilePath();
        m_savedTypeField = cfg.typeFilterField;
        ASSERT_TRUE(m_dir.isValid());
        cfg.SetConfigFilePath((m_dir.path() + "/config.json").toStdString());
        cfg.GetMutableColumns() = {{"type", true, 80}, {"msg", true, 80}};
        cfg.typeFilterField = "type";
        m_view = std::make_unique<EventsTableView>(m_events);
    }
    void TearDown() override
    {
        m_view.reset();
        auto& cfg = config::GetConfig();
        cfg.GetMutableColumns() = m_savedColumns;
        cfg.SetConfigFilePath(m_savedPath);
        cfg.typeFilterField = m_savedTypeField;
    }

    void Load(const std::vector<std::pair<const char*, const char*>>& rows)
    {
        std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
        for (std::size_t i = 0; i < rows.size(); ++i)
            batch.push_back({static_cast<int>(i + 1),
                             {{"type", rows[i].first}, {"msg", rows[i].second}}});
        m_events.AddEventBatch(std::move(batch));
        m_view->RefreshView();
    }

    QTableWidget* Table(PatternAnalysisPanel& panel, int col, const char* header) const
    {
        for (auto* t : panel.findChildren<QTableWidget*>())
            if (t->columnCount() > col && t->horizontalHeaderItem(col)
                && t->horizontalHeaderItem(col)->text() == header)
                return t;
        return nullptr;
    }

    QTemporaryDir                     m_dir;
    db::EventsContainer               m_events;
    std::unique_ptr<EventsTableView>  m_view;
    std::vector<config::ColumnConfig> m_savedColumns;
    std::string                       m_savedPath;
    std::string                       m_savedTypeField;
};

TEST_F(PatternAnalysisPanelTest, TemplateClickFiltersEventsAndKeepsTheTemplateList)
{
    Load({{"OPEN", "open file 1"}, {"CLOSE", "close"}, {"OPEN", "open file 2"}});
    PatternAnalysisPanel panel(m_events, m_view.get());
    panel.Refresh();

    auto* templates = Table(panel, 1, "Template");
    ASSERT_NE(templates, nullptr);
    ASSERT_EQ(templates->rowCount(), 2);
    int openRow = -1;
    for (int r = 0; r < templates->rowCount(); ++r)
        if (templates->item(r, 0)->text() == "OPEN") openRow = r;
    ASSERT_GE(openRow, 0);

    emit templates->cellClicked(openRow, 1);
    const auto* base = m_view->GetBaseFilteredIndices();
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(*base, (std::vector<unsigned long>{0, 2}));

    panel.Refresh(); // the refresh caused by the template filter
    EXPECT_EQ(templates->rowCount(), 2);
}

TEST_F(PatternAnalysisPanelTest, ExactlyNEventsYieldOneNgram)
{
    Load({{"A", "x"}, {"B", "y"}}); // two events, bigrams are the default
    PatternAnalysisPanel panel(m_events, m_view.get());
    panel.Refresh();

    auto* ngrams = Table(panel, 0, "Sequence");
    ASSERT_NE(ngrams, nullptr);
    EXPECT_EQ(ngrams->rowCount(), 1);
}

} // namespace ui::qt::test
