#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>
#include <QComboBox>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <unordered_map>

namespace db {
class EventsContainer;
}

namespace ui::qt {
class EventsTableView;
}

namespace ui::qt {

/**
 * @brief Enhanced bookmark and annotation panel (v1.10.0).
 *
 * Improvements:
 * - **Categories**: Organize bookmarks by type (Bug, TODO, Performance, etc.)
 * - **Color coding**: Each category has a color for visual organization
 * - **Search/Filter**: Find bookmarks by keyword or category
 * - **Sorting**: Sort by row, category, timestamp, or label
 * - **Import/Export**: Save/load bookmark sets as JSON files
 * - **Statistics**: Quick count by category
 *
 * Users can mark events with labels and assign categories. Bookmarks
 * persist in JSON across sessions with category and color metadata.
 */
class BookmarksPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit BookmarksPanel(db::EventsContainer& events,
                            EventsTableView*     eventsView,
                            QWidget*             parent = nullptr);

  public slots:
    /// Bookmark the given event row (called from the events-table context menu).
    void AddBookmarkForRow(int actualRow);

  public:
    nlohmann::json GetSessionData() const;
    void LoadSessionData(const nlohmann::json& data);

  signals:
    /// Emitted when the user activates a bookmark.  MainWindow switches to the
    /// Events tab and scrolls to @p actualRow.
    void NavigateToEvent(int actualRow);

  private slots:
    void OnCurrentRowChanged(int actualRow);
    void OnAddBookmark();
    void OnGoTo();
    void OnRemove();
    void OnExport();
    void OnImport();
    void OnSearchTextChanged(const QString& text);
    void OnCategoryFilterChanged(const QString& category);
    void OnAddCategory();
    void OnRemoveCategory();

  private:
    struct Category
    {
        std::string name;
        std::string color;      // Hex color code
        int count {0};          // Count of bookmarks in this category
    };

    struct Bookmark
    {
        int         row      {-1}; ///< Actual (unfiltered) event row index
        std::string label;         ///< User-provided annotation
        std::string category;      ///< Category name (e.g., "Bug", "TODO")
        std::string summary;       ///< Truncated message/text field preview
        std::string timestamp;     ///< Detected timestamp value
    };

    void BuildLayout();
    void DoAddBookmark(int actualRow, const std::string& label, const std::string& category);
    void RebuildTable();
    void RebuildCategoryFilter();
    void UpdateCategoryCounts();

    /// Returns the selected table row, or -1 if nothing is selected.
    [[nodiscard]] int SelectedTableRow() const;

    /// Apply search and category filters to visible bookmarks
    void ApplyFilters();

    /// Get default categories
    static std::vector<Category> GetDefaultCategories();

    db::EventsContainer& m_events;
    EventsTableView*     m_eventsView;
    int                  m_currentRow {-1}; ///< Currently selected event in events view

    // Categories
    std::vector<Category> m_categories;
    std::unordered_map<std::string, int> m_categoryColorMap;

    QLineEdit*    m_labelEdit         {nullptr};
    QComboBox*    m_categoryCombo     {nullptr};
    QPushButton*  m_addCategoryBtn    {nullptr};
    QPushButton*  m_removeCategoryBtn {nullptr};
    QPushButton*  m_addBtn            {nullptr};
    QTableWidget* m_table             {nullptr};
    QLineEdit*    m_searchEdit        {nullptr};
    QComboBox*    m_categoryFilterCombo {nullptr};
    QPushButton*  m_goToBtn           {nullptr};
    QPushButton*  m_removeBtn         {nullptr};
    QPushButton*  m_exportBtn         {nullptr};
    QPushButton*  m_importBtn         {nullptr};
    QLabel*       m_statusLabel       {nullptr};

    std::vector<Bookmark> m_bookmarks;
    std::vector<Bookmark> m_filteredBookmarks;  // After search/category filtering
};

} // namespace ui::qt
