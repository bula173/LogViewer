#pragma once

#include "IView.hpp"
#include "IEventsView.hpp"

#include <QTableView>
#include <QString>
#include <cstdint>
#include <memory>
#include <vector>

class QMenu;

namespace db
{
class EventsContainer;
}

namespace ui::qt
{

class DrillDownFilter;
class EventsTableModel;
class FilterHeaderView;

class EventsTableView : public QTableView,
                        public ui::IEventsListView,
                        public mvc::IView
{
    Q_OBJECT

  public:
    EventsTableView(db::EventsContainer& events, QWidget* parent = nullptr);
    ~EventsTableView() override;

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
    /// The indices last passed to SetFilteredEvents() (nullptr after
    /// ClearFilter()), without column filters or sorting applied.
    const std::vector<unsigned long>* GetBaseFilteredIndices() const;
    /// True if a filter is active, even if it currently matches zero events.
    bool IsFilterActive() const;

    /// Excel-style per-column value filters (opened from the filter button in
    /// each column header). They are ANDed with each other and with every
    /// filter set through SetFilteredEvents()/ClearFilter().
    bool HasColumnFilters() const;
    void ClearColumnFilters();

    /// Adds the quick filters for the clicked @p cell to @p menu: show only /
    /// exclude its value, a time window around its event and the conversation
    /// of its sender and receiver (see QuickFilters.hpp).
    void AddQuickFilterActions(QMenu& menu, const QModelIndex& cell);
    /// Narrows the rows to the events within ±@p seconds of @p actualRow's
    /// timestamp, on top of the filter set by the rest of the application
    /// (a new window replaces the previous one). False if the event has no
    /// parseable timestamp.
    bool ShowTimeWindow(int actualRow, int seconds);
    /// True while the rows are narrowed by ShowTimeWindow() and nobody replaced
    /// that filter since.
    bool HasTimeWindow() const;
    /// Puts back the filter that was set before ShowTimeWindow().
    void ClearTimeWindow();

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
    void ShowColumnFilterPopup(int column);
    void ResizeColumnsToConfiguration();
    void ScrollToMatchIndex(int matchIndex);
    void RestoreColumnOrder();
    void SaveColumnOrder() const;
    void RestoreColumnWidths();
    void SaveColumnWidths() const;
    /// A model reset drops the selection: remember it by event (before the
    /// reset) and select the same events again afterwards.
    void CaptureSelectionBeforeReset();
    void RestoreSelectionAfterReset();
    /// Keeps the "n of m" search position on the same match after the rows change.
    void OnSearchMatchesChanged();

    /// Returns actual event indices for all currently selected table rows.
    std::vector<int> SelectedActualIndices() const;
    void CopySelectedRowsAsJson();
    void CopySelectedRowsAsCsv();

    db::EventsContainer& m_events;
    EventsTableModel* m_model {nullptr};
    FilterHeaderView* m_filterHeader {nullptr};
    std::unique_ptr<DrillDownFilter> m_timeWindow; ///< see ShowTimeWindow()
    int m_currentMatchIndex {-1};
    int m_currentMatchActual {-1}; ///< event of the current search match

    // Selection carried across a model reset (see CaptureSelectionBeforeReset)
    int              m_resetCurrentActual {-1};
    std::vector<int> m_resetSelectedActual;
    std::uint64_t    m_resetGeneration {0};
    bool             m_restoringSelection {false};
};

} // namespace ui::qt
