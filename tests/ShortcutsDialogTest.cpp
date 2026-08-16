/**
 * @file ShortcutsDialogTest.cpp
 * @brief Tests that ShortcutsDialog's table reflects ShortcutManager state.
 */
#include <gtest/gtest.h>
#include <QAction>
#include <QApplication>
#include <QTableWidget>

#include "ui/qt/dialogs/ShortcutsDialog.hpp"
#include "ui/qt/utils/ShortcutManager.hpp"
#include "Config.hpp"

#include <filesystem>
#include <memory>

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

std::filesystem::path KeybindingsFilePath()
{
    return config::GetConfig().GetDefaultAppPath() / "keybindings.json";
}
} // namespace

class ShortcutsDialogTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        std::filesystem::remove(KeybindingsFilePath());
        ShortcutManager::getInstance().Clear();
        m_action = std::make_unique<QAction>("Do Thing", nullptr);
        m_action->setShortcut(QKeySequence("Ctrl+D"));
        ShortcutManager::getInstance().Register("test.doThing", "Test", "Do Thing", m_action.get());
    }

    void TearDown() override
    {
        ShortcutManager::getInstance().Clear();
        std::filesystem::remove(KeybindingsFilePath());
    }

    std::unique_ptr<QAction> m_action;
};

TEST_F(ShortcutsDialogTest, TableIncludesRegisteredShortcut)
{
    ShortcutsDialog dialog;
    auto* table = dialog.findChild<QTableWidget*>("shortcutsTable");
    ASSERT_NE(table, nullptr);

    bool found = false;
    for (int row = 0; row < table->rowCount(); ++row)
    {
        if (table->item(row, 1)->text() == "Do Thing")
        {
            EXPECT_EQ(table->item(row, 0)->text(), "Test");
            EXPECT_EQ(table->item(row, 2)->text(), QKeySequence("Ctrl+D").toString(QKeySequence::NativeText));
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ShortcutsDialogTest, TableRefreshesAfterExternalRebind)
{
    ASSERT_TRUE(ShortcutManager::getInstance().SetShortcut("test.doThing", QKeySequence("Ctrl+Shift+D")).isOk());

    ShortcutsDialog dialog;
    auto* table = dialog.findChild<QTableWidget*>("shortcutsTable");
    ASSERT_NE(table, nullptr);

    bool found = false;
    for (int row = 0; row < table->rowCount(); ++row)
    {
        if (table->item(row, 1)->text() == "Do Thing")
        {
            EXPECT_EQ(table->item(row, 2)->text(), QKeySequence("Ctrl+Shift+D").toString(QKeySequence::NativeText));
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

} // namespace ui::qt::test
