#pragma once

#include "IView.hpp"
#include "IEventsView.hpp"

#include <QTableView>
#include <QString>
#include <vector>

namespace db
{
class EventsContainer;
}

namespace ui::qt
{

class EventsTableModel;

class EventsTableView : public QTableView,
                        public ui::IEventsListView,
                        public mvc::IView
{
    Q_OBJECT

  public:
    EventsTableView(db::EventsContainer& events, QWidget* parent = nullptr);

    void RefreshColumns() override;
    void RefreshView() override;
    void SetFilteredEvents(
        const std::vector<unsigned long>& filteredIndices) override;
    void ClearFilter() override;
    void UpdateColors() override;
    /// Returns nullptr if no filter is active, otherwise a pointer to the
    /// (possibly empty) filtered indices — callers must not treat a non-null
    /// empty result the same as "no filter" (see IsFilterActive()).
    const std::vector<unsigned long>* GetFilteredIndices() const;
    /// True if a filter is active, even if it currently matches zero events.
    bool IsFilterActive() const;

    void OnDataUpdated() override;
    void OnCurrentIndexUpdated(const int index) override;

    int CurrentActualRow() const;
    void ScrollToActualRow(int actualRow, bool takeFocus = true);

    /// Converts a model row index to the corresponding EventsContainer index
    /// (handles filtering: when filtering is active, model rows != container indices)
    int ResolveToActualIndex(int modelRow) const;

    /// Returns the actual-container row of the first fully visible row,
    /// or -1 if the view has no model or no visible rows.
    int FirstVisibleActualRow() const;

    /// Scrolls the viewport so that @p actualRow is visible at the top
    /// without changing the current selection or stealing focus.
    void SyncScrollTo(int actualRow);

    // ── Search / highlight ────────────────────────────────────────────────
    void SetSearchTerm(const QString& term, bool caseSensitive);
    void NavigateToNextMatch();
    void NavigateToPrevMatch();

  Q_SIGNALS:
    void CurrentActualRowChanged(int actualRow);
    void MatchInfoChanged(int current, int total);
    /// Emitted when the user chooses "Bookmark Event" from the context menu.
    void BookmarkRequested(int actualRow);
    /// Emitted when the user chooses "Add to Scenario" from the context menu.
    void AddToScenarioRequested(int actualRow);

  public Q_SLOTS:
    /// Opens an input dialog and scrolls to the event with the nearest timestamp.
    void JumpToTimestamp();

  private Q_SLOTS:
    /// Called when user drags column header to reorder
    void OnColumnMoved();

  private:
    void InitializeView();
    void ConnectSelectionSignals();
    void ShowContextMenu(const QPoint& pos);
    void ResizeColumnsToConfiguration();
    void ScrollToMatchIndex(int matchIndex);
    void RestoreColumnOrder();
    void SaveColumnOrder() const;
    void RestoreColumnWidths();
    void SaveColumnWidths() const;

    /// Returns actual event indices for all currently selected table rows.
    std::vector<int> SelectedActualIndices() const;
    void CopySelectedRowsAsJson();
    void CopySelectedRowsAsCsv();

    db::EventsContainer& m_events;
    EventsTableModel* m_model {nullptr};
    int m_currentMatchIndex {-1};
};

} // namespace ui::qt
