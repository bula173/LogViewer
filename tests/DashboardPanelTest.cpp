#include <gtest/gtest.h>
#include <QApplication>
#include <QLabel>

#include "qt/panels/DashboardPanel.hpp"
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

class DashboardPanelTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        EnsureQApplication();
        m_savedTypeFilterField = config::GetConfig().typeFilterField;
        m_events = std::make_unique<db::EventsContainer>();
    }

    void TearDown() override
    {
        config::GetConfig().typeFilterField = m_savedTypeFilterField;
    }

    void AddTestEvent(const std::string& level, const std::string& actor, const std::string& message)
    {
        static int eventId = 1;
        m_events->AddEvent(db::LogEvent(eventId++, {
            {"level", level},
            {"actor", actor},
            {"message", message},
            {"timestamp", "2024-01-01T12:00:00Z"}
        }));
    }

    std::unique_ptr<db::EventsContainer> m_events;
    std::string m_savedTypeFilterField;
};

TEST_F(DashboardPanelTest, BreakdownUsesConfiguredTypeFilterField)
{
    config::GetConfig().typeFilterField = "level";
    AddTestEvent("ERROR", "Service-A", "Critical");
    AddTestEvent("ERROR", "Service-B", "Another error");
    AddTestEvent("WARN", "Service-A", "Warning");

    DashboardPanel panel;
    panel.SetEventsSource(m_events.get());

    auto* breakdown = panel.findChild<QLabel*>("dashboardTypeBreakdownLabel");
    ASSERT_NE(breakdown, nullptr);
    const QString text = breakdown->text();
    EXPECT_TRUE(text.contains("ERROR: 2"));
    EXPECT_TRUE(text.contains("WARN: 1"));
}

TEST_F(DashboardPanelTest, BreakdownFollowsNonDefaultTypeFilterField)
{
    // A type filter field other than "level" must drive the breakdown --
    // this is the behavior that was missing before: the panel used to
    // hardcode findByKey("level") regardless of config.
    config::GetConfig().typeFilterField = "actor";
    AddTestEvent("INFO", "WebServer", "Request received");
    AddTestEvent("INFO", "WebServer", "Response sent");
    AddTestEvent("INFO", "Database", "Query executed");

    DashboardPanel panel;
    panel.SetEventsSource(m_events.get());

    auto* breakdown = panel.findChild<QLabel*>("dashboardTypeBreakdownLabel");
    ASSERT_NE(breakdown, nullptr);
    const QString text = breakdown->text();
    EXPECT_TRUE(text.contains("WebServer: 2"));
    EXPECT_TRUE(text.contains("Database: 1"));
    // Must not fall back to counting by "level" when a different field is configured.
    EXPECT_FALSE(text.contains("INFO: 3"));
}

TEST_F(DashboardPanelTest, EmptyContainerShowsNoData)
{
    config::GetConfig().typeFilterField = "level";

    DashboardPanel panel;
    panel.SetEventsSource(m_events.get());

    auto* breakdown = panel.findChild<QLabel*>("dashboardTypeBreakdownLabel");
    ASSERT_NE(breakdown, nullptr);
    EXPECT_EQ(breakdown->text(), "(No data yet)");
}

} // namespace ui::qt::test
