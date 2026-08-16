/**
 * @file ShortcutManagerTest.cpp
 * @brief Tests for ShortcutManager's registration, conflict detection,
 *        reset, and persistence.
 */
#include <gtest/gtest.h>
#include <QAction>
#include <QApplication>

#include "ui/qt/utils/ShortcutManager.hpp"
#include "Config.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
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

class ShortcutManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // QAction requires a QApplication to already exist, so the actions
        // can't be constructed as plain member initializers (those run
        // before SetUp()) — build them here instead.
        EnsureQApplication();
        std::filesystem::remove(KeybindingsFilePath());
        ShortcutManager::getInstance().Clear();
        m_actionA = std::make_unique<QAction>("Action A", nullptr);
        m_actionB = std::make_unique<QAction>("Action B", nullptr);
    }

    void TearDown() override
    {
        ShortcutManager::getInstance().Clear();
        std::filesystem::remove(KeybindingsFilePath());
    }

    // Actions must outlive the registry entries referencing them.
    std::unique_ptr<QAction> m_actionA;
    std::unique_ptr<QAction> m_actionB;
};

TEST_F(ShortcutManagerTest, RegisterAppliesDefaultShortcut)
{
    m_actionA->setShortcut(QKeySequence("Ctrl+A"));
    ShortcutManager::getInstance().Register("test.a", "Test", "Action A", m_actionA.get());

    const auto all = ShortcutManager::getInstance().GetAll();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].id, "test.a");
    EXPECT_EQ(all[0].sequence, QKeySequence("Ctrl+A"));
    EXPECT_EQ(all[0].defaultSequence, QKeySequence("Ctrl+A"));
}

TEST_F(ShortcutManagerTest, SetShortcutRebindsAction)
{
    m_actionA->setShortcut(QKeySequence("Ctrl+A"));
    ShortcutManager::getInstance().Register("test.a", "Test", "Action A", m_actionA.get());

    const auto result = ShortcutManager::getInstance().SetShortcut("test.a", QKeySequence("Ctrl+Shift+A"));
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(m_actionA->shortcut(), QKeySequence("Ctrl+Shift+A"));
}

TEST_F(ShortcutManagerTest, SetShortcutRejectsConflict)
{
    m_actionA->setShortcut(QKeySequence("Ctrl+A"));
    m_actionB->setShortcut(QKeySequence("Ctrl+B"));
    ShortcutManager::getInstance().Register("test.a", "Test", "Action A", m_actionA.get());
    ShortcutManager::getInstance().Register("test.b", "Test", "Action B", m_actionB.get());

    const auto result = ShortcutManager::getInstance().SetShortcut("test.a", QKeySequence("Ctrl+B"));
    EXPECT_FALSE(result.isOk());
    // Action A keeps its original binding — the rejected assignment must not apply.
    EXPECT_EQ(m_actionA->shortcut(), QKeySequence("Ctrl+A"));
}

TEST_F(ShortcutManagerTest, ResetToDefaultRestoresOriginalShortcut)
{
    m_actionA->setShortcut(QKeySequence("Ctrl+A"));
    ShortcutManager::getInstance().Register("test.a", "Test", "Action A", m_actionA.get());
    ASSERT_TRUE(ShortcutManager::getInstance().SetShortcut("test.a", QKeySequence("Ctrl+Z")).isOk());

    ShortcutManager::getInstance().ResetToDefault("test.a");
    EXPECT_EQ(m_actionA->shortcut(), QKeySequence("Ctrl+A"));
}

TEST_F(ShortcutManagerTest, SetShortcutPersistsOverrideToDisk)
{
    m_actionA->setShortcut(QKeySequence("Ctrl+A"));
    m_actionB->setShortcut(QKeySequence("Ctrl+B"));
    ShortcutManager::getInstance().Register("test.a", "Test", "Action A", m_actionA.get());
    ShortcutManager::getInstance().Register("test.b", "Test", "Action B", m_actionB.get());
    ASSERT_TRUE(ShortcutManager::getInstance().SetShortcut("test.a", QKeySequence("Ctrl+Z")).isOk());

    std::ifstream ifs(KeybindingsFilePath());
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json j;
    ifs >> j;

    // Only the entry that actually differs from its default is persisted.
    EXPECT_EQ(j.value("test.a", ""), QKeySequence("Ctrl+Z").toString().toStdString());
    EXPECT_FALSE(j.contains("test.b"));
}

TEST_F(ShortcutManagerTest, ResetToDefaultRemovesPersistedOverride)
{
    m_actionA->setShortcut(QKeySequence("Ctrl+A"));
    ShortcutManager::getInstance().Register("test.a", "Test", "Action A", m_actionA.get());
    ASSERT_TRUE(ShortcutManager::getInstance().SetShortcut("test.a", QKeySequence("Ctrl+Z")).isOk());

    ShortcutManager::getInstance().ResetToDefault("test.a");

    std::ifstream ifs(KeybindingsFilePath());
    ASSERT_TRUE(ifs.is_open());
    nlohmann::json j;
    ifs >> j;
    EXPECT_FALSE(j.contains("test.a"));
}

} // namespace ui::qt::test
