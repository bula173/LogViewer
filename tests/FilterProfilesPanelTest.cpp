/**
 * @file FilterProfilesPanelTest.cpp
 * @brief Tests for FilterProfilesPanel's Export/Import feature.
 *
 * Uses QTest's synthetic-event utilities (QTest::mouseClick, QTest::keyClicks)
 * to drive real widgets in-process — these post QMouseEvent/QKeyEvent objects
 * directly into the target widget, never touching the OS input queue or
 * screen, so they can't accidentally interact with any other window and need
 * no OS accessibility permissions. This is the correct way to test Qt UI,
 * as opposed to OS-level UI automation (osascript/System Events), which
 * operates on whatever's actually on screen and isn't reliably scoped to one
 * application.
 */
#include <gtest/gtest.h>
#include <QApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include "qt/panels/FilterProfilesPanel.hpp"
#include "Config.hpp"

#include <filesystem>
#include <fstream>

namespace ui::qt::test {

namespace {
// Only one QApplication may exist per process; guard construction in case
// another test file in this binary also needs one.
void EnsureQApplication()
{
    if (QApplication::instance()) return;
    static int argc = 1;
    static char argv0[] = "tests";
    static char* argv[] = {argv0};
    static QApplication app(argc, argv);
}

// FilterProfilesPanel persists to the real app-data "filter_profiles.json"
// (there's no test-specific override), so each test must remove it before
// constructing a panel — otherwise state leaks across tests and across runs.
std::filesystem::path ProfilesFilePath()
{
    return config::GetConfig().GetDefaultAppPath() / "filter_profiles.json";
}
} // namespace

class FilterProfilesPanelTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        std::filesystem::remove(ProfilesFilePath());
    }

    void TearDown() override { std::filesystem::remove(ProfilesFilePath()); }

    static FilterProfile MakeProfile(const std::string& name)
    {
        FilterProfile p;
        p.name = name;
        p.timeRange.field = "timestamp";
        p.timeRange.active = false;
        p.hasTypeFilter = true;
        p.checkedTypes = {"ERROR", "WARN"};
        return p;
    }
};

// ---------------------------------------------------------------------------
// ExportProfileToPath / ReadProfilesFile — pure I/O, no dialog involved
// ---------------------------------------------------------------------------

TEST_F(FilterProfilesPanelTest, ExportThenReadRoundTripsProfile)
{
    const auto path = QString::fromStdString(
        (std::filesystem::temp_directory_path() / "fpp_export_test.filters.json").string());

    const auto profile = MakeProfile("RoundTrip");
    const auto exportResult = FilterProfilesPanel::ExportProfileToPath(profile, path);
    ASSERT_TRUE(exportResult.isOk());

    auto readResult = FilterProfilesPanel::ReadProfilesFile(path);
    ASSERT_TRUE(readResult.isOk());
    const auto profiles = readResult.unwrap();
    ASSERT_EQ(profiles.size(), 1u);
    EXPECT_EQ(profiles[0].name, "RoundTrip");
    EXPECT_TRUE(profiles[0].hasTypeFilter);
    EXPECT_EQ(profiles[0].checkedTypes, (std::vector<std::string>{"ERROR", "WARN"}));

    std::filesystem::remove(path.toStdString());
}

TEST_F(FilterProfilesPanelTest, ReadProfilesFileRejectsMissingFile)
{
    const auto result = FilterProfilesPanel::ReadProfilesFile("/nonexistent/path.filters.json");
    EXPECT_FALSE(result.isOk());
}

TEST_F(FilterProfilesPanelTest, ReadProfilesFileRejectsNonArrayJson)
{
    const auto path = std::filesystem::temp_directory_path() / "fpp_bad.filters.json";
    { std::ofstream ofs(path); ofs << "{\"not\": \"an array\"}"; }

    const auto result = FilterProfilesPanel::ReadProfilesFile(QString::fromStdString(path.string()));
    EXPECT_FALSE(result.isOk());

    std::filesystem::remove(path);
}

// ---------------------------------------------------------------------------
// MergeProfiles — collision handling
// ---------------------------------------------------------------------------

TEST_F(FilterProfilesPanelTest, MergeProfilesAddsNewProfiles)
{
    FilterProfilesPanel panel;
    EXPECT_EQ(panel.ProfileCount(), 0u);

    const size_t applied = panel.MergeProfiles({MakeProfile("A"), MakeProfile("B")}, /*overwrite=*/false);
    EXPECT_EQ(applied, 2u);
    EXPECT_EQ(panel.ProfileCount(), 2u);
    EXPECT_TRUE(panel.HasProfile("A"));
    EXPECT_TRUE(panel.HasProfile("B"));
}

TEST_F(FilterProfilesPanelTest, MergeProfilesSkipsCollisionsWhenNotOverwriting)
{
    FilterProfilesPanel panel;
    panel.MergeProfiles({MakeProfile("Existing")}, false);
    ASSERT_EQ(panel.ProfileCount(), 1u);

    auto incoming = MakeProfile("Existing");
    incoming.checkedTypes = {"CHANGED"};
    const size_t applied = panel.MergeProfiles({incoming, MakeProfile("New")}, /*overwrite=*/false);

    EXPECT_EQ(applied, 1u); // only "New" applied; "Existing" skipped
    EXPECT_EQ(panel.ProfileCount(), 2u);
}

TEST_F(FilterProfilesPanelTest, MergeProfilesOverwritesCollisionsWhenRequested)
{
    FilterProfilesPanel panel;
    panel.MergeProfiles({MakeProfile("Existing")}, false);

    auto incoming = MakeProfile("Existing");
    incoming.checkedTypes = {"CHANGED"};
    const size_t applied = panel.MergeProfiles({incoming}, /*overwrite=*/true);

    EXPECT_EQ(applied, 1u);
    EXPECT_EQ(panel.ProfileCount(), 1u); // replaced in place, not duplicated
}

// ---------------------------------------------------------------------------
// Real widget interaction via QTest — no OS-level automation
// ---------------------------------------------------------------------------

TEST_F(FilterProfilesPanelTest, ClickingSaveButtonEmitsSaveRequestedWithTypedName)
{
    FilterProfilesPanel panel;
    QSignalSpy saveSpy(&panel, &FilterProfilesPanel::SaveRequested);

    auto* nameEdit = panel.findChild<QLineEdit*>("filterProfileNameEdit");
    auto* saveBtn  = panel.findChild<QPushButton*>("filterProfileSaveButton");
    ASSERT_NE(nameEdit, nullptr);
    ASSERT_NE(saveBtn, nullptr);

    QTest::keyClicks(nameEdit, "MyProfile");
    QTest::mouseClick(saveBtn, Qt::LeftButton);

    ASSERT_EQ(saveSpy.count(), 1);
    EXPECT_EQ(saveSpy.takeFirst().at(0).toString(), QString("MyProfile"));
}

TEST_F(FilterProfilesPanelTest, ExportButtonDisabledUntilProfileSelected)
{
    FilterProfilesPanel panel;
    panel.StoreProfile(MakeProfile("Selectable"));

    auto* exportBtn = panel.findChild<QPushButton*>("filterProfileExportButton");
    auto* list = panel.findChild<QListWidget*>("filterProfilesList");
    ASSERT_NE(exportBtn, nullptr);
    ASSERT_NE(list, nullptr);
    ASSERT_EQ(list->count(), 1);

    EXPECT_FALSE(exportBtn->isEnabled());

    list->setCurrentRow(0);
    EXPECT_TRUE(exportBtn->isEnabled());
}

} // namespace ui::qt::test
