#pragma once

#include "mvc/IView.hpp"
#include "ui/IEventsView.hpp"

#include <QTableView>
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

  signals:
    void CurrentActualRowChanged(int actualRow);

  private:
    void InitializeView();
    void ConnectSelectionSignals();
    void ResizeColumnsToConfiguration();

    db::EventsContainer& m_events;
    EventsTableModel* m_model {nullptr};
};

} // namespace ui::qt
