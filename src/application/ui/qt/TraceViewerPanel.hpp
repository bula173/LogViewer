#pragma once

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QWidget>

#include <map>
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
 * @brief Correlation / trace viewer panel.
 *
 * Groups visible events by the value of a user-selected field (e.g.
 * @c requestId, @c sessionId, @c traceId).  Each unique value becomes a row
 * showing aggregate statistics: event count, error count, duration, and
 * first/last seen timestamps.  Double-clicking a row filters the events view
 * to that correlation group.
 *
 * Refreshes automatically via a modelReset connection wired up in MainWindow.
 */
class TraceViewerPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit TraceViewerPanel(db::EventsContainer& events,
                              EventsTableView*     eventsView,
                              QWidget*             parent = nullptr);

  public slots:
    /// Rebuild the correlation table from the current visible row set.
    void Refresh();

  private:
    void BuildLayout();

    /// Populate the field combo from the first N events' field names.
    void PopulateFieldCombo();

    /// Rebuild the tree for the given correlation @p field.
    void RebuildTree(const QString& field);

    db::EventsContainer& m_events;
    EventsTableView*     m_eventsView;

    QComboBox*   m_fieldCombo  {nullptr};
    QTreeWidget* m_tree        {nullptr};
    QPushButton* m_clearBtn    {nullptr};
    QLabel*      m_statusLabel {nullptr};

    /// Per-trace event indices populated during RebuildTree; consumed by double-click.
    std::map<std::string, std::vector<unsigned long>> m_traceEvents;
};

} // namespace ui::qt
