/**
 * @file ShortcutManager.hpp
 * @brief Central registry of user-customizable keyboard shortcuts.
 *
 * @details
 * Menu-building code (MainWindow::SetupMenus()) calls @c Register() right
 * after creating each QAction that carries an application shortcut. The
 * manager applies any previously-saved custom shortcut immediately, and
 * later lets @c ShortcutsDialog list, edit, and reset shortcuts through
 * @c GetAll() / @c SetShortcut() / @c ResetToDefault().
 *
 * Overrides are persisted as `<appdata>/keybindings.json`, a flat map of
 * shortcut id to key sequence string. Only entries that differ from their
 * default are stored.
 */
#pragma once

#include "Error.hpp"
#include "Result.hpp"

#include <QKeySequence>
#include <QString>

#include <string>
#include <variant>
#include <vector>

class QAction;

namespace ui::qt {

using ShortcutResult = util::Result<std::monostate, error::Error>;

/// Snapshot of a single registered shortcut, for display/editing.
struct ShortcutInfo
{
    std::string id;
    QString category;
    QString label;
    QKeySequence sequence;
    QKeySequence defaultSequence;
};

class ShortcutManager
{
  public:
    static ShortcutManager& getInstance();

    /**
     * @brief Registers @p action under @p id, applying any saved override.
     *
     * @p action's current shortcut (set by the caller before this call) is
     * treated as the default to reset back to. Calling this again with an
     * id that's already registered updates the entry in place (the action
     * pointer is refreshed and its shortcut is left as-is).
     */
    void Register(const std::string& id, const QString& category,
        const QString& label, QAction* action);

    /// All currently registered shortcuts, in registration order.
    [[nodiscard]] std::vector<ShortcutInfo> GetAll() const;

    /**
     * @brief Assigns @p sequence to the shortcut identified by @p id.
     *
     * Rejected (Err) if @p sequence is already used by a different
     * registered shortcut and @p sequence is non-empty. An empty sequence
     * clears the shortcut (unbinds the action).
     */
    [[nodiscard]] ShortcutResult SetShortcut(const std::string& id, const QKeySequence& sequence);

    /// Restores the shortcut identified by @p id to its default.
    void ResetToDefault(const std::string& id);

    /// Restores every registered shortcut to its default.
    void ResetAllToDefaults();

    /**
     * @brief Clears the registry (not the persisted overrides file).
     *
     * Intended for tests: without this, QAction pointers from a previous
     * test's destroyed widgets would dangle in the process-wide singleton.
     */
    void Clear();

  private:
    ShortcutManager();
    ~ShortcutManager() = default;

    struct Entry
    {
        std::string id;
        QString category;
        QString label;
        QAction* action {nullptr};
        QKeySequence defaultSequence;
    };

    void LoadOverrides();
    void SaveOverrides() const;
    static std::string DefaultFilePath();

    std::vector<Entry> m_entries;
    std::vector<std::pair<std::string, QKeySequence>> m_pendingOverrides;
};

} // namespace ui::qt
