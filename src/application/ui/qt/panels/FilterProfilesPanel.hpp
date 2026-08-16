/**
 * @file FilterProfilesPanel.hpp
 * @brief Named filter-profile save/restore panel and FilterProfile data type.
 *
 * @details
 * A **filter profile** is a lightweight snapshot of the complete filter state:
 *  - Time range (field name + from/to bounds + active flag)
 *  - Actor check state (set of unchecked actor keys)
 *  - Type filter selection (optional; only stored when at least one type exists)
 *
 * Profiles are persisted as JSON in the application data directory
 * (`<appdata>/filter_profiles.json`) and are loaded automatically on start-up.
 *
 * ### Interaction model
 * The panel does **not** directly access other filter panels.  Instead it
 * delegates through MainWindow via two signals:
 *  - @c SaveRequested(name)           → MainWindow gathers state → @c StoreProfile()
 *  - @c ProfileLoadRequested(profile) → MainWindow applies each field to the
 *    appropriate panel
 *
 * ### Actor key encoding
 * Each entry in @c FilterProfile::uncheckedActors is a composite string of the
 * form `"<defName>\0<actorName>"`.  The NUL character serves as a delimiter
 * that cannot appear in either component.  Use @c ActorKey::Encode() /
 * @c ActorKey::Decode() for safe construction and decomposition.
 *
 * @author LogViewer Development Team
 * @date 2026
 */
#pragma once

#include "TimeRangeFilterPanel.hpp"
#include "Error.hpp"
#include "Result.hpp"

#include <nlohmann/json.hpp>

#include <QWidget>
#include <set>
#include <string>
#include <vector>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace ui::qt {

// ---------------------------------------------------------------------------
// FilterProfile data structure
// ---------------------------------------------------------------------------

/**
 * @brief Snapshot of all active filter state that can be named and persisted.
 *
 * A profile captures the complete, user-visible filter state at a single
 * point in time so that it can be restored later with a single click.
 *
 * ### Covered filter dimensions
 * | Field             | Restored by                                        |
 * |-------------------|----------------------------------------------------|
 * | @c timeRange      | TimeRangeFilterPanel::SetState() + Apply()/Clear() |
 * | @c uncheckedActors| ActorsPanel::RestoreUncheckedActors()              |
 * | @c checkedTypes   | TypeFilterView::SetCheckedTypes()                  |
 *
 * ### JSON schema
 * @code{.json}
 * {
 *   "name": "Production errors",
 *   "timeRange": { "field": "timestamp", "from": "2026-01-01", "to": "", "active": true },
 *   "uncheckedActors": ["MyDef\u0000ServerA"],
 *   "hasTypeFilter": true,
 *   "checkedTypes": ["ERROR", "FATAL"]
 * }
 * @endcode
 *
 * @note Actor keys use the @c ActorKey::Encode() / @c ActorKey::Decode()
 *       helpers defined in ActorDefinition.hpp.
 */
struct FilterProfile
{
    std::string name; ///< Human-readable profile name (used as the unique identifier)

    // Time range ──────────────────────────────────────────────────────────
    /// Time range filter state; @c active=false means "no time filter".
    TimeRangeFilterPanel::State timeRange;

    // Actor filter ────────────────────────────────────────────────────────
    /// Encoded actor keys (@c ActorKey::Encode(defName, actorName)) that
    /// were unchecked when the profile was saved.  An empty vector means all
    /// actors were checked.
    std::vector<std::string> uncheckedActors;

    // Type filter ─────────────────────────────────────────────────────────
    /// @c true when @c checkedTypes should be applied on load.
    /// @c false leaves the type filter panel untouched.
    bool hasTypeFilter {false};

    /// Log-event type strings that were checked when the profile was saved.
    /// Only meaningful when @c hasTypeFilter is @c true.
    std::vector<std::string> checkedTypes;

    // ── JSON round-trip ─────────────────────────────────────────────────
    /// Serialises the profile to a JSON object.
    [[nodiscard]] nlohmann::json ToJson() const;

    /// Deserialises a profile from a JSON object.
    /// Missing fields receive safe default values.
    [[nodiscard]] static FilterProfile FromJson(const nlohmann::json& j);
};

// ---------------------------------------------------------------------------
// FilterProfilesPanel widget
// ---------------------------------------------------------------------------

/**
 * @brief Panel for saving and restoring named filter profiles.
 *
 * Profiles capture the complete filter state (time range, actor checks,
 * type filter) and persist them to a JSON file in the app config directory.
 *
 * @par Responsibility split
 * The panel is deliberately kept thin.  It knows nothing about the other
 * filter panels.  All cross-panel coordination is delegated to MainWindow
 * via the two signals below.
 *
 * @par Persistence
 * Profiles are stored in `<appdata>/filter_profiles.json` (one JSON array).
 * The file is loaded in the constructor and saved after every mutating
 * operation (save / delete).
 *
 * @par Double-click shortcut
 * Double-clicking a list item is equivalent to clicking **Load**.
 *
 * @par Export / Import
 * **Export** writes the selected profile to a user-chosen `*.filters.json`
 * file (same JSON array format as internal storage, just one entry) for
 * sharing with teammates. **Import** reads a `*.filters.json` file (one or
 * more profiles) and merges it into the current list, prompting once if any
 * imported name collides with an existing profile.
 *
 * @par Testability
 * The QFileDialog calls that pick the export/import path live only in the
 * @c HandleExport() / @c HandleImport() slots. The actual file I/O
 * (@c ExportProfileToPath(), @c ReadProfilesFile()) and the merge logic
 * (@c MergeProfiles()) are separate public methods that take a path or a
 * profile list directly, so they can be exercised in tests without going
 * through a native file picker.
 */
class FilterProfilesPanel : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the panel, builds its layout, and loads persisted profiles.
     * @param parent Optional Qt parent widget.
     */
    explicit FilterProfilesPanel(QWidget* parent = nullptr);

    /**
     * @brief Stores (or replaces) a profile in the panel and persists it to disk.
     *
     * Called by MainWindow after it has gathered state from all filter panels
     * in response to the @c SaveRequested signal.  If a profile with the same
     * @c name already exists it is replaced in-place; otherwise the profile is
     * appended.
     *
     * @param profile The complete filter state to persist.
     */
    void StoreProfile(const FilterProfile& profile);

    /// Result of exporting a single profile to a file.
    using ExportResult = util::Result<std::monostate, error::Error>;
    /// Result of reading a .filters.json file: the profiles it contains.
    using ReadProfilesResult = util::Result<std::vector<FilterProfile>, error::Error>;

    /**
     * @brief Writes @p profile to @p path as a single-entry .filters.json array.
     *
     * Pure I/O — does not touch this panel's own profile list or persisted
     * storage, so it can be tested directly with a temp path.
     */
    [[nodiscard]] static ExportResult ExportProfileToPath(
        const FilterProfile& profile, const QString& path);

    /**
     * @brief Reads and parses a .filters.json file (one or more profiles).
     *
     * Pure I/O — does not touch this panel's own state. Entries with an
     * empty name or a name duplicated within the file are silently skipped
     * (matches the same rule LoadFromFile() applies to internal storage).
     */
    [[nodiscard]] static ReadProfilesResult ReadProfilesFile(const QString& path);

    /**
     * @brief Merges @p incoming into this panel's profile list and persists
     * the result to internal storage.
     *
     * Profiles whose name matches one already in the list are skipped
     * unless @p overwriteCollisions is true, in which case the existing
     * profile is replaced in place.
     *
     * @return The number of profiles actually added or overwritten.
     */
    size_t MergeProfiles(std::vector<FilterProfile> incoming, bool overwriteCollisions);

    /// Number of profiles currently held by the panel.
    [[nodiscard]] size_t ProfileCount() const { return m_profiles.size(); }

    /// Whether a profile with the given name currently exists.
    [[nodiscard]] bool HasProfile(const std::string& name) const;

signals:
    /**
     * @brief Emitted when the user clicks **Save current state**.
     *
     * The receiver (MainWindow) should gather filter state from all panels,
     * build a @c FilterProfile, set its @c name to @p name, and call
     * @c StoreProfile().
     *
     * @param name The profile name entered by the user (already trimmed).
     */
    void SaveRequested(const QString& name);

    /**
     * @brief Emitted when the user selects a profile and clicks **Load**
     *        (or double-clicks the list item).
     *
     * The receiver (MainWindow) should apply @p profile to all filter panels.
     *
     * @param profile The selected profile to restore.
     */
    void ProfileLoadRequested(const FilterProfile& profile);

private slots:
    void HandleSave();
    void HandleLoad();
    void HandleDelete();
    void HandleSelectionChanged();
    void HandleExport();
    void HandleImport();

private:
    void BuildLayout();
    void RebuildList();
    void SaveToFile();
    void LoadFromFile();
    static std::string DefaultFilePath();

    std::vector<FilterProfile> m_profiles;
    QListWidget* m_list       {nullptr};
    QLineEdit*   m_nameEdit   {nullptr};
    QPushButton* m_saveBtn    {nullptr};
    QPushButton* m_loadBtn    {nullptr};
    QPushButton* m_deleteBtn  {nullptr};
    QPushButton* m_exportBtn  {nullptr};
    QPushButton* m_importBtn  {nullptr};
    QLabel*      m_statusLabel{nullptr};
};

} // namespace ui::qt
