#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace db {
class EventsContainer;
}

namespace ui::qt {
class EventsTableView;
}

namespace ui::qt {

/**
 * @brief Bookmark and annotation panel.
 *
 * Users can mark any selected event with an optional text label.  Bookmarks
 * are listed in a table showing the event row, label, timestamp, and a brief
 * message preview.  Double-clicking a row (or clicking **Go To**) scrolls the
 * events view to that event.  The full list can be exported as a Markdown
 * table for incident reports.  Bookmarks persist in QSettings across sessions.
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

  private:
    struct Bookmark
    {
        int         row      {-1}; ///< Actual (unfiltered) event row index
        std::string label;         ///< User-provided annotation
        std::string summary;       ///< Truncated message/text field preview
        std::string timestamp;     ///< Detected timestamp value
    };

    void BuildLayout();
    void DoAddBookmark(int actualRow, const std::string& label);
    void RebuildTable();

    /// Returns the selected table row, or -1 if nothing is selected.
    [[nodiscard]] int SelectedTableRow() const;

    db::EventsContainer& m_events;
    EventsTableView*     m_eventsView;
    int                  m_currentRow {-1}; ///< Currently selected event in events view

    QLineEdit*    m_labelEdit   {nullptr};
    QPushButton*  m_addBtn      {nullptr};
    QTableWidget* m_table       {nullptr};
    QPushButton*  m_goToBtn     {nullptr};
    QPushButton*  m_removeBtn   {nullptr};
    QPushButton*  m_exportBtn   {nullptr};
    QLabel*       m_statusLabel {nullptr};

    std::vector<Bookmark> m_bookmarks;
};

} // namespace ui::qt
