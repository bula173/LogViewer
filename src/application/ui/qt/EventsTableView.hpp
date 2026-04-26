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
    void UpdateColors() override;
    const std::vector<unsigned long>* GetFilteredIndices() const;

    void OnDataUpdated() override;
    void OnCurrentIndexUpdated(const int index) override;

    int CurrentActualRow() const;
    void ScrollToActualRow(int actualRow);

    // ── Search / highlight ────────────────────────────────────────────────
    void SetSearchTerm(const QString& term, bool caseSensitive);
    void NavigateToNextMatch();
    void NavigateToPrevMatch();

  signals:
    void CurrentActualRowChanged(int actualRow);
    void MatchInfoChanged(int current, int total);

  private:
    void InitializeView();
    void ConnectSelectionSignals();
    void ResizeColumnsToConfiguration();
    void ScrollToMatchIndex(int matchIndex);

    db::EventsContainer& m_events;
    EventsTableModel* m_model {nullptr};
    int m_currentMatchIndex {-1};
};

} // namespace ui::qt
