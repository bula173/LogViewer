// The dashboard must show real data for logs whose columns are not named
// "actor" / the configured type field (e.g. safeAPI merged logs).
#include <gtest/gtest.h>
#include <QApplication>
#include <QDir>
#include <QFile>
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

TEST(DashboardPanelDataTest, ShowsFileInfoTimeRangeTypesAndActorsForSourceDestinationLogs)
{
    EnsureQApplication();

    const QString path = QDir::temp().filePath("lv_dashboard_test_log.txt");
    {
        QFile f(path);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(2048, 'x'));
    }

    db::EventsContainer events;
    for (int i = 1; i <= 10; ++i)
    {
        events.AddEvent(db::LogEvent(i, {
            {"timestamp", "2026-09-19T06:00:" + std::string(i < 10 ? "0" : "") + std::to_string(i) + "Z"},
            {"source", i % 2 ? "RBC West" : "RBC East"},
            {"destination", i % 3 == 0 ? "internal" : "a,b"},
            {"event_type", i % 2 ? "MOVE" : "STOP"}}));
    }

    DashboardPanel panel;
    panel.SetEventsSource(&events);
    panel.SetFilePath(path);
    panel.UpdateStats();
    const QString text = AllText(panel);
    QFile::remove(path);

    EXPECT_TRUE(text.contains("lv_dashboard_test_log.txt")) << text.toStdString();
    EXPECT_TRUE(text.contains("TXT"));
    EXPECT_TRUE(text.contains("2026-09-19T06:00:01Z"));                 // time range start
    EXPECT_TRUE(text.contains("2026-09-19T06:00:10Z"));                 // time range end
    EXPECT_TRUE(text.contains("MOVE"));                                 // type breakdown fell back to event_type
    EXPECT_TRUE(text.contains("RBC West"));                             // top actors from source/destination
    EXPECT_TRUE(text.contains("a (")) << text.toStdString();            // comma list split into actors
    EXPECT_FALSE(text.contains("internal"));                            // placeholder is not an actor
    EXPECT_FALSE(text.contains("No actors"));
    EXPECT_FALSE(text.contains("No events have"));
}

} // namespace ui::qt::test
