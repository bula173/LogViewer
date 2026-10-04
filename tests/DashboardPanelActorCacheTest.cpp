// The dashboard caches the discovered actor columns; a new log with the same
// number of events but other columns must be discovered again.
#include <gtest/gtest.h>
#include <QApplication>
#include <QLabel>

#include "qt/panels/DashboardPanel.hpp"
#include "EventsContainer.hpp"

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

QString AllText(const QWidget& w)
{
    QStringList texts;
    for (const auto* label : w.findChildren<QLabel*>())
        texts << label->text();
    return texts.join('\n');
}
} // namespace

TEST(DashboardPanelActorCacheTest, SameSizedNewLogRediscoversActorColumns)
{
    EnsureQApplication();
    db::EventsContainer events;
    for (int i = 1; i <= 10; ++i)
        events.AddEvent(db::LogEvent(i, {{"source", i % 2 ? "RBC West" : "RBC East"},
                                         {"destination", i % 2 ? "RBC East" : "RBC West"}}));

    DashboardPanel panel;
    panel.SetEventsSource(&events);
    ASSERT_TRUE(AllText(panel).contains("RBC West")) << AllText(panel).toStdString();

    events.Clear(); // another file, also 10 events, other actor columns
    for (int i = 1; i <= 10; ++i)
        events.AddEvent(db::LogEvent(i, {{"sender", i % 2 ? "Alpha" : "Beta"},
                                         {"receiver", i % 2 ? "Beta" : "Alpha"}}));
    panel.UpdateStats();

    const QString text = AllText(panel);
    EXPECT_TRUE(text.contains("Alpha")) << text.toStdString();
    EXPECT_FALSE(text.contains("RBC West")) << text.toStdString();
}

} // namespace ui::qt::test
