#pragma once

#include "IStatisticsStrategy.hpp"

#include <QChartView>
#include <QComboBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QWidget>
#include <memory>
#include <vector>

class QDateTime;

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
 * Four sections, all scoped to the filtered (visible) row set:
 *  1. Summary        — compact key/value grid: counts, time range, event rate
 *  2. Event Types    — horizontal bar chart: count per type-field value
 *  3. Top N Values   — frequency table for any chosen column, with % share
 *  4. Field Statistics — per-column unique value count and fill rate
 *
 * Bar charts expose hover tooltips with full label text and exact counts so
 * that truncated axis labels never hide information.
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
    // ── Layout builder ────────────────────────────────────────────────────
    void BuildLayout();

    // ── Per-section refresh (all receive the already-computed index list) ─
    void RefreshFormatStats   (const std::vector<unsigned long>& indices);
    void RefreshSummaryTable  (const std::vector<unsigned long>& indices);
    void RefreshTypeChart     (const std::vector<unsigned long>& indices);
    void RefreshTopNTable     (const std::vector<unsigned long>& indices);
    void RefreshFieldStats    (const std::vector<unsigned long>& indices);

    /// Select the appropriate statistics strategy based on current event data.
    [[nodiscard]] std::unique_ptr<IStatisticsStrategy> SelectStrategy() const;

    /// Populates the column combo from the current model header names.
    void PopulateColumnCombo();

    /// Returns the indices of currently visible events.
    /// If no filter is active the full range [0, Size()) is returned.
    [[nodiscard]] std::vector<unsigned long> VisibleIndices() const;

    // ── Static helpers ────────────────────────────────────────────────────
    /// Return the name of the first field whose values parse as timestamps,
    /// or an empty string if no such field is found.
    [[nodiscard]] static std::string DetectTimestampField(
        db::EventsContainer&              events,
        const std::vector<unsigned long>& indices);

    // ── Widgets ───────────────────────────────────────────────────────────
    db::EventsContainer& m_events;
    EventsTableView*      m_eventsView;

    QGroupBox*    m_formatStatsGroup {nullptr}; ///< Hidden for non-CAN formats
    QTableWidget* m_formatStatsTable {nullptr}; ///< Format-specific rows from strategy
    QTableWidget* m_summaryTable     {nullptr}; ///< Key/value summary grid
    QChartView*   m_typeChartView    {nullptr}; ///< Event-type horizontal bars
    QComboBox*    m_columnCombo      {nullptr}; ///< Column selector for Top-N
    QSpinBox*     m_topNSpin         {nullptr}; ///< How many top values to show
    QTableWidget* m_topNTable        {nullptr}; ///< Top-N value frequency table
    QTableWidget* m_fieldStatsTable  {nullptr}; ///< Per-field unique/fill stats
};

} // namespace ui::qt
