// RunExplorerDialog: rows, verdict filter, grouping by message, open requests
// and the background scan of a results folder.
#include <gtest/gtest.h>

#include "qt/dialogs/RunExplorerDialog.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

#include <fstream>
#include <string>
#include <vector>

namespace ui::qt::test {

namespace {

using analyzer::TestRunEntry;
using analyzer::TestVerdict;

void EnsureQApplication()
{
    if (QApplication::instance()) return;
    static int argc = 1;
    static char argv0[] = "tests";
    static char* argv[] = {argv0};
    static QApplication app(argc, argv);
}

TestRunEntry Entry(const std::string& name, TestVerdict verdict, const std::string& message = {})
{
    TestRunEntry e;
    e.file           = "/run/" + name + "__merged_logs.txt";
    e.testName       = name;
    e.verdict        = verdict;
    e.durationUs     = 1'500'000;
    e.entries        = 10;
    e.failureMessage = message;
    if (!message.empty())
        e.failureSource = analyzer::FailureSource::StepFail;
    return e;
}

/// In run order: PASS, FAIL (20s), FAIL (setup), FAIL (40s).
std::vector<TestRunEntry> SampleRun()
{
    return {Entry("A", TestVerdict::Pass),
            Entry("B", TestVerdict::Fail, "24 not received within 20s"),
            Entry("C", TestVerdict::Fail, "suite setup failed"),
            Entry("D", TestVerdict::Fail, "24 not received within 40s")};
}

int VisibleTopLevel(const QTreeWidget* tree)
{
    int n = 0;
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        n += tree->topLevelItem(i)->isHidden() ? 0 : 1;
    return n;
}

QTreeWidgetItem* FindTest(QTreeWidget* tree, const QString& name)
{
    const auto found = tree->findItems(name, Qt::MatchExactly | Qt::MatchRecursive, 2);
    return found.isEmpty() ? nullptr : found.first();
}

} // namespace

TEST(RunExplorerDialogTest, ListsOneRowPerTestInRunOrder)
{
    EnsureQApplication();
    RunExplorerDialog dialog;
    dialog.SetEntries("/run", SampleRun());

    auto* tree = dialog.findChild<QTreeWidget*>("runExplorerTree");
    ASSERT_EQ(tree->topLevelItemCount(), 4);
    EXPECT_EQ(tree->topLevelItem(0)->text(2), "A");
    EXPECT_EQ(tree->topLevelItem(1)->text(1), "FAIL");
    EXPECT_EQ(tree->topLevelItem(1)->text(3), "1.5 s");
    EXPECT_EQ(tree->topLevelItem(1)->text(4), "10");
    EXPECT_EQ(tree->topLevelItem(1)->text(5), "24 not received within 20s");
    EXPECT_EQ(tree->topLevelItem(1)->text(6), "B__merged_logs.txt");
    EXPECT_TRUE(dialog.findChild<QLabel*>("runExplorerSummary")->text().contains("4 tests, 3 failed"));
}

TEST(RunExplorerDialogTest, VerdictFilterHidesOtherTests)
{
    EnsureQApplication();
    RunExplorerDialog dialog;
    dialog.SetEntries("/run", SampleRun());
    auto* tree   = dialog.findChild<QTreeWidget*>("runExplorerTree");
    auto* filter = dialog.findChild<QComboBox*>("runExplorerVerdictFilter");

    filter->setCurrentIndex(filter->findData(static_cast<int>(TestVerdict::Fail)));
    EXPECT_EQ(VisibleTopLevel(tree), 3);
    filter->setCurrentIndex(filter->findData(static_cast<int>(TestVerdict::Pass)));
    EXPECT_EQ(VisibleTopLevel(tree), 1);
    filter->setCurrentIndex(filter->findData(-1));
    EXPECT_EQ(VisibleTopLevel(tree), 4);
}

TEST(RunExplorerDialogTest, GroupingCollapsesFailuresWithTheSameNormalisedMessage)
{
    EnsureQApplication();
    RunExplorerDialog dialog;
    dialog.SetEntries("/run", SampleRun());
    auto* tree = dialog.findChild<QTreeWidget*>("runExplorerTree");

    dialog.findChild<QCheckBox*>("runExplorerGroupByMessage")->setChecked(true);
    // Groups "within Ns" (B, D) and "suite setup failed" (C), plus the PASS row.
    ASSERT_EQ(tree->topLevelItemCount(), 3);
    QTreeWidgetItem* big = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
        if (tree->topLevelItem(i)->childCount() == 2)
            big = tree->topLevelItem(i);
    ASSERT_NE(big, nullptr);
    EXPECT_TRUE(big->text(2).startsWith("2"));
    EXPECT_EQ(big->text(5), "24 not received within 20s");
    EXPECT_EQ(big->child(0)->text(2), "B");
    EXPECT_EQ(big->child(1)->text(2), "D");

    // Passed only: the groups disappear with their failed members.
    auto* filter = dialog.findChild<QComboBox*>("runExplorerVerdictFilter");
    filter->setCurrentIndex(filter->findData(static_cast<int>(TestVerdict::Pass)));
    EXPECT_EQ(VisibleTopLevel(tree), 1);

    dialog.findChild<QCheckBox*>("runExplorerGroupByMessage")->setChecked(false);
    EXPECT_EQ(tree->topLevelItemCount(), 4);
}

TEST(RunExplorerDialogTest, ActivatingATestRequestsItsLog)
{
    EnsureQApplication();
    RunExplorerDialog dialog;
    dialog.SetEntries("/run", SampleRun());
    auto* tree = dialog.findChild<QTreeWidget*>("runExplorerTree");
    QSignalSpy spy(&dialog, &RunExplorerDialog::OpenTestRequested);

    QTreeWidgetItem* c = FindTest(tree, "C");
    ASSERT_NE(c, nullptr);
    tree->setCurrentItem(c);
    emit tree->itemActivated(c, 0);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "/run/C__merged_logs.txt");
}

TEST(RunExplorerDialogTest, OpenWithPreviousUsesTheTestThatRanBefore)
{
    EnsureQApplication();
    RunExplorerDialog dialog;
    dialog.SetEntries("/run", SampleRun());
    auto* tree     = dialog.findChild<QTreeWidget*>("runExplorerTree");
    auto* previous = dialog.findChild<QAction*>("runExplorerOpenWithPrevious");
    QSignalSpy spy(&dialog, &RunExplorerDialog::OpenWithPreviousRequested);

    tree->setCurrentItem(FindTest(tree, "A"));
    EXPECT_FALSE(previous->isEnabled()); // the first test has no predecessor

    // Sorting by name must not change which test ran before.
    tree->sortByColumn(2, Qt::DescendingOrder);
    tree->setCurrentItem(FindTest(tree, "C"));
    ASSERT_TRUE(previous->isEnabled());
    previous->trigger();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "/run/B__merged_logs.txt");
    EXPECT_EQ(spy.at(0).at(1).toString(), "B");
    EXPECT_EQ(spy.at(0).at(2).toString(), "/run/C__merged_logs.txt");
    EXPECT_EQ(spy.at(0).at(3).toString(), "C");
}

TEST(RunExplorerDialogTest, OpenFolderScansInTheBackground)
{
    EnsureQApplication();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const auto write = [&dir](const std::string& file, const std::string& name, const std::string& status,
                              const std::string& ts) {
        std::ofstream out((dir.path() + "/").toStdString() + file, std::ios::binary);
        out << "# FULL MERGED TEST STEPS, CONTAINER & SIMULATOR LOGS\n"
               "# Test Case   : " << name << "\n# Status      : " << status << "\n"
               "# Timestamp   : " << ts << "\n# Entries     : 1\n"
               "[" << ts << "] [IO] [robot] [internal] [INFO] [TEST_START] [Test execution started] [name="
            << name << "]\n";
    };
    write("Second__merged_logs.txt", "Second", "FAIL", "2026-10-03T22:35:00.000000Z");
    write("First__merged_logs.txt", "First", "PASS", "2026-10-03T22:34:00.000000Z");

    RunExplorerDialog dialog;
    QSignalSpy finished(&dialog, &RunExplorerDialog::ScanFinished);
    dialog.OpenFolder(dir.path());
    ASSERT_TRUE(finished.wait(10000));

    ASSERT_EQ(dialog.Entries().size(), 2u);
    EXPECT_EQ(dialog.Entries()[0].testName, "First");
    EXPECT_EQ(dialog.Entries()[1].testName, "Second");
    EXPECT_EQ(dialog.findChild<QTreeWidget*>("runExplorerTree")->topLevelItemCount(), 2);
}

} // namespace ui::qt::test
