#include "KeyboardNavigationManager.hpp"
#include "Logger.hpp"

#include <QKeyEvent>
#include <QWidget>
#include <QApplication>
#include <QTabWidget>
#include <QTableView>

namespace ui::qt::utils
{

KeyboardNavigationManager::KeyboardNavigationManager(QWidget* mainWindow)
    : QObject(mainWindow)
    , m_mainWindow(mainWindow)
{
    setupDefaultShortcuts();
}

void KeyboardNavigationManager::setupDefaultShortcuts()
{
    // Panel navigation
    registerShortcut(QKeySequence("Ctrl+Tab"), "Next panel",
        [this]() { navigateToNextPanel(); });
    registerShortcut(QKeySequence("Ctrl+Shift+Tab"), "Previous panel",
        [this]() { navigateToPreviousPanel(); });

    // Table navigation
    registerShortcut(QKeySequence(Qt::Key_Down), "Navigate down",
        [this]() { navigateDown(); });
    registerShortcut(QKeySequence(Qt::Key_Up), "Navigate up",
        [this]() { navigateUp(); });
    registerShortcut(QKeySequence(Qt::Key_Home), "Jump to first",
        [this]() { jumpToFirst(); });
    registerShortcut(QKeySequence(Qt::Key_End), "Jump to last",
        [this]() { jumpToLast(); });

    // Search & find
    registerShortcut(QKeySequence::Find, "Find in current view",
        [this]() {
            util::Logger::Debug("[Navigation] Find shortcut triggered");
        });

    util::Logger::Info("[KeyboardNavigationManager] Registered {} shortcuts",
        m_shortcutDescriptions.size());
}

void KeyboardNavigationManager::registerShortcut(const QKeySequence& key,
    const QString& description, std::function<void()> handler)
{
    if (!key.isEmpty())
    {
        // Store using the first key code
        m_shortcuts[key[0]] = handler;
        m_shortcutDescriptions[key.toString()] = description;
    }
}

bool KeyboardNavigationManager::handleKeyPress(QKeyEvent* event)
{
    if (!event)
        return false;

    if (m_vimKeybindingsEnabled)
    {
        if (handleVimBindings(event))
            return true;
    }

    return handleStandardBindings(event);
}

bool KeyboardNavigationManager::handleVimBindings(QKeyEvent* event)
{
    int key = event->key();

    // Vim navigation: hjkl
    if (key == Qt::Key_H)
    {
        navigateLeft();
        return true;
    }
    if (key == Qt::Key_J)
    {
        navigateDown();
        return true;
    }
    if (key == Qt::Key_K)
    {
        navigateUp();
        return true;
    }
    if (key == Qt::Key_L)
    {
        navigateRight();
        return true;
    }

    // Vim jump: gg (jump to first)
    if (key == Qt::Key_G)
    {
        if (m_vimGGPressed)
        {
            jumpToFirst();
            m_vimGGPressed = false;
            return true;
        }
        m_vimGGPressed = true;
        return true;
    }

    // Vim jump: G (jump to last)
    if (key == Qt::Key_G && (event->modifiers() & Qt::ShiftModifier))
    {
        jumpToLast();
        m_vimGGPressed = false;
        return true;
    }

    // Reset vim mode on any other key
    m_vimGGPressed = false;
    return false;
}

bool KeyboardNavigationManager::handleStandardBindings(QKeyEvent* event)
{
    int key = event->key();
    int modifiers = static_cast<int>(event->modifiers());

    // Check registered shortcuts
    for (const auto& [shortcutKey, handler] : m_shortcuts)
    {
        if (shortcutKey == (modifiers | key))
        {
            handler();
            return true;
        }
    }

    return false;
}

void KeyboardNavigationManager::navigateToNextPanel()
{
    // Find current focused widget
    QWidget* focused = QApplication::focusWidget();
    if (!focused || !m_mainWindow)
        return;

    // Find parent tab widget
    QTabWidget* tabs = nullptr;
    QWidget* parent = focused;
    while (parent)
    {
        tabs = qobject_cast<QTabWidget*>(parent);
        if (tabs)
            break;
        parent = parent->parentWidget();
    }

    if (tabs && tabs->count() > 0)
    {
        int nextIndex = (tabs->currentIndex() + 1) % tabs->count();
        tabs->setCurrentIndex(nextIndex);
    }

    util::Logger::Debug("[Navigation] Switched to next panel");
}

void KeyboardNavigationManager::navigateToPreviousPanel()
{
    QWidget* focused = QApplication::focusWidget();
    if (!focused || !m_mainWindow)
        return;

    QTabWidget* tabs = nullptr;
    QWidget* parent = focused;
    while (parent)
    {
        tabs = qobject_cast<QTabWidget*>(parent);
        if (tabs)
            break;
        parent = parent->parentWidget();
    }

    if (tabs && tabs->count() > 0)
    {
        int prevIndex = (tabs->currentIndex() - 1 + tabs->count()) % tabs->count();
        tabs->setCurrentIndex(prevIndex);
    }

    util::Logger::Debug("[Navigation] Switched to previous panel");
}

void KeyboardNavigationManager::navigateDown()
{
    QWidget* focused = QApplication::focusWidget();
    if (!focused)
        return;

    QTableView* table = qobject_cast<QTableView*>(focused);
    if (!table || !table->model())
        return;

    int row = table->currentIndex().row() + 1;
    if (row < table->model()->rowCount())
    {
        table->setCurrentIndex(table->model()->index(row, 0));
    }
}

void KeyboardNavigationManager::navigateUp()
{
    QWidget* focused = QApplication::focusWidget();
    if (!focused)
        return;

    QTableView* table = qobject_cast<QTableView*>(focused);
    if (!table || !table->model())
        return;

    int row = table->currentIndex().row() - 1;
    if (row >= 0)
    {
        table->setCurrentIndex(table->model()->index(row, 0));
    }
}

void KeyboardNavigationManager::navigateLeft()
{
    QWidget* focused = QApplication::focusWidget();
    if (!focused)
        return;

    QTableView* table = qobject_cast<QTableView*>(focused);
    if (!table || !table->model())
        return;

    int col = table->currentIndex().column() - 1;
    if (col >= 0)
    {
        table->setCurrentIndex(table->model()->index(table->currentIndex().row(), col));
    }
}

void KeyboardNavigationManager::navigateRight()
{
    QWidget* focused = QApplication::focusWidget();
    if (!focused)
        return;

    QTableView* table = qobject_cast<QTableView*>(focused);
    if (!table || !table->model())
        return;

    int col = table->currentIndex().column() + 1;
    if (col < table->model()->columnCount())
    {
        table->setCurrentIndex(table->model()->index(table->currentIndex().row(), col));
    }
}

void KeyboardNavigationManager::jumpToFirst()
{
    QWidget* focused = QApplication::focusWidget();
    if (!focused)
        return;

    QTableView* table = qobject_cast<QTableView*>(focused);
    if (!table || !table->model())
        return;

    if (table->model()->rowCount() > 0)
    {
        table->setCurrentIndex(table->model()->index(0, 0));
        table->scrollToTop();
    }

    util::Logger::Debug("[Navigation] Jumped to first row");
}

void KeyboardNavigationManager::jumpToLast()
{
    QWidget* focused = QApplication::focusWidget();
    if (!focused)
        return;

    QTableView* table = qobject_cast<QTableView*>(focused);
    if (!table || !table->model())
        return;

    int lastRow = table->model()->rowCount() - 1;
    if (lastRow >= 0)
    {
        table->setCurrentIndex(table->model()->index(lastRow, 0));
        table->scrollToBottom();
    }

    util::Logger::Debug("[Navigation] Jumped to last row");
}

void KeyboardNavigationManager::jumpToRow(int row)
{
    QWidget* focused = QApplication::focusWidget();
    if (!focused)
        return;

    QTableView* table = qobject_cast<QTableView*>(focused);
    if (!table || !table->model())
        return;

    if (row >= 0 && row < table->model()->rowCount())
    {
        table->setCurrentIndex(table->model()->index(row, 0));
        table->scrollTo(table->model()->index(row, 0));
    }

    util::Logger::Debug("[Navigation] Jumped to row {}", row);
}

}  // namespace ui::qt::utils
