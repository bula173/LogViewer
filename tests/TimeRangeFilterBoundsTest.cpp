// Time range filter bounds: "to" is inclusive at its own precision and a 'T'
// or a space between date and time compare alike.
#include <gtest/gtest.h>
#include <QApplication>
#include <QTemporaryDir>

#include "qt/panels/TimeRangeFilterPanel.hpp"
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

class TimeRangeFilterBoundsTest : public ::testing::Test
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
        cfg.GetMutableColumns() = {{"timestamp", true, 80}};

        const char* stamps[] = {
            "2026-01-01 09:59:59",     // 0
            "2026-01-01 10:00:00.250", // 1
            "2026-01-01T10:00:01",     // 2
            "2026-01-01 23:59:59.999", // 3
            "2026-01-02 00:00:00",     // 4
        };
        std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
        for (int i = 0; i < 5; ++i)
            batch.push_back({i + 1, {{"timestamp", stamps[i]}}});
        m_events.AddEventBatch(std::move(batch));
        m_view  = std::make_unique<EventsTableView>(m_events);
        m_panel = std::make_unique<TimeRangeFilterPanel>(m_events, m_view.get());
    }
    void TearDown() override
    {
        m_panel.reset();
        m_view.reset();
        config::GetConfig().GetMutableColumns() = m_savedColumns;
        config::GetConfig().SetConfigFilePath(m_savedPath);
    }

    std::vector<unsigned long> Apply(const char* from, const char* to)
    {
        m_panel->SetState({"timestamp", from, to, true});
        m_panel->Apply();
        const auto* base = m_view->GetBaseFilteredIndices();
        return base ? *base : std::vector<unsigned long>{};
    }

    QTemporaryDir                         m_dir;
    db::EventsContainer                   m_events;
    std::unique_ptr<EventsTableView>      m_view;
    std::unique_ptr<TimeRangeFilterPanel> m_panel;
    std::vector<config::ColumnConfig>     m_savedColumns;
    std::string                           m_savedPath;
};

TEST_F(TimeRangeFilterBoundsTest, ToIncludesFractionsOfItsSecond)
{
    EXPECT_EQ(Apply("2026-01-01 10:00:00", "2026-01-01 10:00:00"),
              (std::vector<unsigned long>{1}));
}

TEST_F(TimeRangeFilterBoundsTest, DateOnlyToIncludesTheWholeDay)
{
    EXPECT_EQ(Apply("", "2026-01-01"), (std::vector<unsigned long>{0, 1, 2, 3}));
}

TEST_F(TimeRangeFilterBoundsTest, TAndSpaceSeparatorsCompareAlike)
{
    EXPECT_EQ(Apply("2026-01-01T10:00:00", "2026-01-01T10:00:01"),
              (std::vector<unsigned long>{1, 2}));
}

} // namespace ui::qt::test
