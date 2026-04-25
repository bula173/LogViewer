#pragma once

#include <QChartView>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>
#include <vector>

namespace db {
class EventsContainer;
}

namespace ui::qt {
class EventsTableView;
}

namespace ui::qt {

/**
 * @brief Docked panel showing aggregate statistics for the currently visible events.
 *
 * Three sections, all scoped to the filtered (visible) row set:
 *  1. Event Types  — horizontal bar chart: count per type field value
 *  2. Events Over Time — bar histogram bucketed by timestamp field
 *  3. Top N Values — table of most-frequent values for any chosen column
 *
 * Refreshes automatically via a modelReset connection wired up in MainWindow.
 */
class StatsSummaryPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit StatsSummaryPanel(db::EventsContainer& events,
                               EventsTableView*     eventsView,
                               QWidget*             parent = nullptr);

  public slots:
    /// Recompute all statistics from the current visible row set.
    void Refresh();

  private:
    void BuildLayout();
    void RefreshTypeChart();
    void RefreshTimeChart();
    void RefreshTopNTable();
    void PopulateColumnCombo();

    /// Returns the indices of currently visible events.
    /// If no filter is active the full range [0, Size()) is returned.
    std::vector<unsigned long> VisibleIndices() const;

    db::EventsContainer& m_events;
    EventsTableView*      m_eventsView;

    QLabel*       m_summaryLabel  {nullptr};
    QChartView*   m_typeChartView {nullptr};
    QChartView*   m_timeChartView {nullptr};
    QComboBox*    m_columnCombo   {nullptr};
    QSpinBox*     m_topNSpin      {nullptr};
    QTableWidget* m_topNTable     {nullptr};
};

} // namespace ui::qt
