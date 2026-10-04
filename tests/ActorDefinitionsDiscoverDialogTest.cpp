// The Discover dialog's "Unique values" column counts every distinct value
// of the probed events, not just the (at most 5) sample values.
#include <gtest/gtest.h>
#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>

#include "qt/panels/ActorDefinitionsPanel.hpp"
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
} // namespace

TEST(ActorDefinitionsDiscoverDialogTest, UniqueValuesAreNotCappedAtTheSampleCount)
{
    EnsureQApplication();
    db::EventsContainer events;
    for (int i = 0; i < 40; ++i)
        events.AddEvent(db::LogEvent(i + 1, {{"from", "n" + std::to_string(i % 8)},
                                             {"to",   "n" + std::to_string((i + 1) % 8)}}));

    ActorDefinitionsPanel panel;
    panel.SetEventsSource(&events);

    // Accept the method dialog, then read and cancel the candidates dialog.
    QString fromUnique;
    bool sawCandidates = false;
    QTimer driver;
    QElapsedTimer clock;
    clock.start();
    QObject::connect(&driver, &QTimer::timeout, [&] {
        auto* dlg = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dlg) return;
        if (clock.elapsed() > 15000) { dlg->reject(); return; } // never hang
        if (auto* table = dlg->findChild<QTableWidget*>())
        {
            for (int r = 0; r < table->rowCount(); ++r)
                if (table->item(r, 1)->text() == "from")
                    fromUnique = table->item(r, 2)->text();
            sawCandidates = true;
            dlg->reject();
            driver.stop();
        }
        else
        {
            dlg->accept(); // heuristic discovery is preselected
        }
    });
    driver.start(20);

    QPushButton* discover = nullptr;
    for (auto* b : panel.findChildren<QPushButton*>())
        if (b->text().startsWith("Discover")) discover = b;
    ASSERT_NE(discover, nullptr);
    discover->click(); // returns once both dialogs are closed

    ASSERT_TRUE(sawCandidates);
    EXPECT_EQ(fromUnique, "8");
}

} // namespace ui::qt::test
