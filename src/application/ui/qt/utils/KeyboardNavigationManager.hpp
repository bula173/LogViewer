#pragma once

#include <QObject>
#include <QKeySequence>
#include <functional>
#include <map>

class QWidget;
class QKeyEvent;

namespace ui::qt::utils
{

/**
 * @brief Manages keyboard navigation and shortcuts throughout the application.
 *
 * Provides:
 * - Tab navigation between panels
 * - Arrow key navigation in tables
 * - Vim-like keybindings (hjkl)
 * - Quick access shortcuts
 * - Focus management
 */
class KeyboardNavigationManager : public QObject
{
    Q_OBJECT

  public:
    explicit KeyboardNavigationManager(QWidget* mainWindow);

    /// Register a keyboard shortcut with a handler
    void registerShortcut(const QKeySequence& key, const QString& description,
                         std::function<void()> handler);

    /// Handle key press event
    bool handleKeyPress(QKeyEvent* event);

    /// Navigate to next panel (Ctrl+Tab)
    void navigateToNextPanel();

    /// Navigate to previous panel (Ctrl+Shift+Tab)
    void navigateToPreviousPanel();

    /// Navigate down in table (Down arrow or J)
    void navigateDown();

    /// Navigate up in table (Up arrow or K)
    void navigateUp();

    /// Navigate left in table (Left arrow or H)
    void navigateLeft();

    /// Navigate right in table (Right arrow or L)
    void navigateRight();

    /// Jump to first row (Home or G+G)
    void jumpToFirst();

    /// Jump to last row (End)
    void jumpToLast();

    /// Jump to row by number (: followed by number)
    void jumpToRow(int row);

    /// Enable/disable Vim-like keybindings
    void setVimKeybindingsEnabled(bool enabled) { m_vimKeybindingsEnabled = enabled; }
    bool areVimKeybindingsEnabled() const { return m_vimKeybindingsEnabled; }

    /// Get all registered shortcuts
    const std::map<QString, QString>& getShortcuts() const { return m_shortcutDescriptions; }

  private:
    void setupDefaultShortcuts();
    bool handleVimBindings(QKeyEvent* event);
    bool handleStandardBindings(QKeyEvent* event);

    QWidget* m_mainWindow;
    std::map<int, std::function<void()>> m_shortcuts;  // Qt key code -> handler
    std::map<QString, QString> m_shortcutDescriptions;  // Key -> description
    bool m_vimKeybindingsEnabled {false};
    bool m_vimGGPressed {false};  // Track G key for GG binding
};

}  // namespace ui::qt::utils
