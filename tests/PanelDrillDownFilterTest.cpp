// Timeline bucket click / trace double-click narrow the events view. The
// refresh that follows must keep showing the set the pick was made from, keep
// the clear button usable, and clearing must restore the filter that was set
// before (e.g. a type filter), not drop every filter.
#include <gtest/gtest.h>
#include <QApplication>
#include <QAbstractBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>

#include "qt/panels/TimelineChartPanel.hpp"
#include "qt/panels/TraceViewerPanel.hpp"
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

QPushButton* Button(QWidget* panel, const QString& text)
{
    for (auto* b : panel->findChildren<QPushButton*>())
        if (b->text() == text) return b;
    return nullptr;
}
} // namespace

class PanelDrillDownFilterTest : public ::testing::Test
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
        cfg.GetMutableColumns() = {{"timestamp", true, 80}, {"trace", true, 80}};

        // 12 events, one per minute, in traces t0..t2.
        std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
        for (int i = 0; i < 12; ++i)
            batch.push_back({i + 1, {
                {"timestamp", QString("2026-01-01T10:%1:00").arg(i, 2, 10, QChar('0')).toStdString()},
                {"trace", "t" + std::to_string(i % 3)}}});
        m_events.AddEventBatch(std::move(batch));
        m_view = std::make_unique<EventsTableView>(m_events);

        // An upstream filter (e.g. the type filter): the first 9 events.
        m_view->SetFilteredEvents(m_upstream);
    }
    void TearDown() override
    {
        m_view.reset();
        config::GetConfig().GetMutableColumns() = m_savedColumns;
        config::GetConfig().SetConfigFilePath(m_savedPath);
    }

    const std::vector<unsigned long>  m_upstream {0, 1, 2, 3, 4, 5, 6, 7, 8};
    QTemporaryDir                     m_dir;
    db::EventsContainer               m_events;
    std::unique_ptr<EventsTableView>  m_view;
    std::vector<config::ColumnConfig> m_savedColumns;
    std::string                       m_savedPath;
};

TEST_F(PanelDrillDownFilterTest, TimelineBucketKeepsHistogramAndClearRestoresUpstreamFilter)
{
    TimelineChartPanel panel(m_events, m_view.get());
    panel.Refresh();

    auto* chart = panel.findChild<QChartView*>()->chart();
    ASSERT_FALSE(chart->series().isEmpty());
    auto* series = qobject_cast<QAbstractBarSeries*>(chart->series().first());
    ASSERT_NE(series, nullptr);
    emit series->clicked(0, series->barSets().first()); // first time bucket

    const auto* base = m_view->GetBaseFilteredIndices();
    ASSERT_NE(base, nullptr);
    ASSERT_LT(base->size(), m_upstream.size());

    panel.Refresh(); // the refresh caused by the bucket filter
    auto* clear = Button(&panel, "Clear Selection");
    ASSERT_NE(clear, nullptr);
    EXPECT_TRUE(clear->isEnabled());
    bool shows9 = false;
    for (auto* l : panel.findChildren<QLabel*>())
        shows9 = shows9 || l->text().startsWith("9 events");
    EXPECT_TRUE(shows9) << "histogram must still cover the 9 upstream events";

    clear->click();
    base = m_view->GetBaseFilteredIndices();
    ASSERT_NE(base, nullptr) << "the upstream filter must survive";
    EXPECT_EQ(*base, m_upstream);
}

TEST_F(PanelDrillDownFilterTest, TracePickKeepsTreeAndClearRestoresUpstreamFilter)
{
    TraceViewerPanel panel(m_events, m_view.get());
    panel.Refresh();
    auto* combo = panel.findChild<QComboBox*>();
    ASSERT_NE(combo, nullptr);
    combo->setCurrentText("trace");
    auto* tree = panel.findChild<QTreeWidget*>();
    ASSERT_EQ(tree->topLevelItemCount(), 3);

    emit tree->itemDoubleClicked(tree->topLevelItem(0), 0);
    const auto* base = m_view->GetBaseFilteredIndices();
    ASSERT_NE(base, nullptr);
    ASSERT_EQ(base->size(), 3u);

    panel.Refresh(); // the refresh caused by the trace filter
    EXPECT_EQ(tree->topLevelItemCount(), 3);
    auto* clear = Button(&panel, "Clear Filter");
    ASSERT_NE(clear, nullptr);
    EXPECT_TRUE(clear->isEnabled());

    clear->click();
    base = m_view->GetBaseFilteredIndices();
    ASSERT_NE(base, nullptr) << "the upstream filter must survive";
    EXPECT_EQ(*base, m_upstream);
}

} // namespace ui::qt::test
