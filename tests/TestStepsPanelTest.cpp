// TestStepsPanel: hint for non-test logs, verdict banner, navigation.
#include <gtest/gtest.h>

#include "qt/panels/TestStepsPanel.hpp"
#include "EventsContainer.hpp"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTreeWidget>

#include <string>

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

void Add(db::EventsContainer& events, const std::string& secs, const std::string& type,
         const std::string& info, const std::string& payload)
{
    const int id = static_cast<int>(events.Size()) + 1;
    events.AddEvent(db::LogEvent(id, {{"timestamp", "2026-10-03T22:34:" + secs + "Z"},
                                      {"source", "robot"},
                                      {"event_type", type},
                                      {"info", info},
                                      {"payload", payload}}));
}

/// A failing test: TestStep 1 passes, TestStep 2 fails at row 5.
void AddFailingTest(db::EventsContainer& events)
{
    Add(events, "35.000000", "TEST_START", "Test execution started", "name=ERTMS RBC Handover");  // 0
    Add(events, "35.100000", "LOG", "Log message", "TestStep 1: Connect train 1");                // 1
    Add(events, "35.200000", "STEP", "Connect Train", "'1'");                                       // 2
    Add(events, "39.000000", "LOG", "Log message", "TestStep 2: Establish session");               // 3
    Add(events, "39.100000", "STEP", "Wait For Train Message", "'1', '24', '20s', '0.2s'");         // 4
    Add(events, "59.639434", "STEP_FAIL", "Wait For Train Message", "24 not received within 20s");  // 5
    Add(events, "59.990000", "TEST_END", "Test finished (FAIL)", "name=ERTMS RBC Handover status=FAIL"); // 6
}

} // namespace

TEST(TestStepsPanelTest, LogWithoutTestShowsOnlyTheHint)
{
    EnsureQApplication();
    db::EventsContainer events;
    Add(events, "00.000000", "STATUS", "[c-west | GP] self", "cycle=937 up=1");

    TestStepsPanel panel(events);
    panel.Refresh();
    EXPECT_FALSE(panel.findChild<QLabel*>("testStepsHint")->isHidden());
    EXPECT_TRUE(panel.findChild<QFrame*>("testStepsBanner")->isHidden());
    EXPECT_TRUE(panel.findChild<QTreeWidget*>("testStepsTree")->isHidden());
}

TEST(TestStepsPanelTest, FailedTestShowsBannerAndGoToFailureNavigatesToTheFailure)
{
    EnsureQApplication();
    db::EventsContainer events;
    AddFailingTest(events);

    TestStepsPanel panel(events);
    QSignalSpy spy(&panel, &TestStepsPanel::NavigateToEvent);
    panel.Refresh();

    EXPECT_TRUE(panel.findChild<QLabel*>("testStepsHint")->isHidden());
    const QString banner = panel.findChild<QLabel*>("testStepsBannerText")->text();
    EXPECT_TRUE(banner.contains("FAILED")) << banner.toStdString();
    EXPECT_TRUE(banner.contains("ERTMS RBC Handover"));
    EXPECT_TRUE(banner.contains("Wait For Train Message"));
    EXPECT_TRUE(banner.contains("24 not received within 20s"));

    auto* goToFailure = panel.findChild<QPushButton*>("testStepsGoToFailure");
    ASSERT_FALSE(goToFailure->isHidden());
    goToFailure->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 5);

    auto* tree = panel.findChild<QTreeWidget*>("testStepsTree");
    ASSERT_EQ(tree->topLevelItemCount(), 2);
    EXPECT_TRUE(tree->topLevelItem(1)->isExpanded()); // the failed step is opened
    ASSERT_NE(tree->currentItem(), nullptr);           // …and its failing keyword selected
    EXPECT_TRUE(tree->currentItem()->text(0).contains("Wait For Train Message"));
}

TEST(TestStepsPanelTest, DoubleClickAndEnterJumpToTheStepsFirstEvent)
{
    EnsureQApplication();
    db::EventsContainer events;
    AddFailingTest(events);

    TestStepsPanel panel(events);
    QSignalSpy spy(&panel, &TestStepsPanel::NavigateToEvent);
    panel.Refresh();

    auto* tree = panel.findChild<QTreeWidget*>("testStepsTree");
    QTreeWidgetItem* step1 = tree->topLevelItem(0);
    ASSERT_TRUE(step1->text(0).contains("TestStep 1"));
    emit tree->itemDoubleClicked(step1, 0);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toInt(), 1);

    tree->setCurrentItem(step1->child(0)); // Connect Train
    QTest::keyClick(tree, Qt::Key_Return);
    ASSERT_EQ(spy.count(), 2);
    EXPECT_EQ(spy.at(1).at(0).toInt(), 2);
}

TEST(TestStepsPanelTest, PassedTestShowsPassBannerWithoutGoToFailure)
{
    EnsureQApplication();
    db::EventsContainer events;
    Add(events, "01.000000", "TEST_START", "Test execution started", "name=IL Grants");
    Add(events, "01.100000", "STEP", "Connect Train", "'1'");
    Add(events, "02.000000", "TEST_END", "Test finished (PASS)", "name=IL Grants status=PASS");
    events.SetFileMetadata({{"Test Case", "IL Grants A Movement Authority"}, {"Status", "PASS"}});

    TestStepsPanel panel(events);
    panel.Refresh();
    const QString banner = panel.findChild<QLabel*>("testStepsBannerText")->text();
    EXPECT_TRUE(banner.contains("PASSED")) << banner.toStdString();
    EXPECT_TRUE(banner.contains("IL Grants A Movement Authority")); // header name wins
    EXPECT_TRUE(panel.findChild<QPushButton*>("testStepsGoToFailure")->isHidden());

    // Loading another log (here: clearing and adding a non-test event) updates the tab.
    events.Clear();
    Add(events, "00.000000", "STATUS", "[c-west | GP] self", "cycle=1");
    panel.Refresh();
    EXPECT_FALSE(panel.findChild<QLabel*>("testStepsHint")->isHidden());
}

} // namespace ui::qt::test
