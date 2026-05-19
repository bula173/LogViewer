#pragma once

#include <QChartView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

#include <string>
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
 * @brief Interactive event-timeline panel.
 *
 * Displays a stacked bar chart of visible events distributed over time,
 * colour-coded by log level.  Clicking a bar narrows the events view to
 * the events in that time bucket.  A **Clear Selection** button removes
 * the bucket filter and restores the previous visible set.
 *
 * Refreshes automatically via a modelReset connection wired up in MainWindow.
 */
class TimelineChartPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit TimelineChartPanel(db::EventsContainer& events,
                                EventsTableView*     eventsView,
                                QWidget*             parent = nullptr);

  public slots:
    /// Rebuild the chart from the current visible row set.
    void Refresh();

  private:
    void BuildLayout();

    /// Returns visible event indices (filtered, or all if no filter is active).
    [[nodiscard]] std::vector<unsigned long> VisibleIndices() const;

    /// Parse a string to QDateTime using common log timestamp formats.
    [[nodiscard]] static QDateTime ParseTimestamp(const QString& s);

    /// Return the name of the first field whose values parse as timestamps,
    /// or empty if none is found.
    [[nodiscard]] static std::string DetectTimestampField(
        db::EventsContainer&              events,
        const std::vector<unsigned long>& indices);

    db::EventsContainer& m_events;
    EventsTableView*     m_eventsView;

    QChartView*  m_chartView   {nullptr};
    QSpinBox*    m_bucketSpin  {nullptr};
    QPushButton* m_clearBtn    {nullptr};
    QLabel*      m_statusLabel {nullptr};

    /// Per-bucket event indices populated during Refresh; consumed by click handlers.
    std::vector<std::vector<unsigned long>> m_bucketEvents;
};

} // namespace ui::qt
