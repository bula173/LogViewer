#pragma once

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QWidget>

#include <map>
#include <string>
#include <unordered_map>
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
 * @c requestId, @c sessionId, @c traceId).  Each unique value becomes a
 * top-level row showing aggregate statistics: event count, error count,
 * duration, and first/last timestamps.
 *
 * Child rows show per-actor and per-message-type breakdowns.  When span
 * fields (@c spanId / @c parentSpanId or @c parentId) are present the
 * top-level rows are further nested into a call-tree hierarchy.
 *
 * A search bar at the top filters the tree to matching trace IDs.
 * Double-clicking any row filters the events view to the associated events.
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

    /// Show/hide tree items that do (not) contain @p text.
    void FilterTree(const QString& text);

    db::EventsContainer& m_events;
    EventsTableView*     m_eventsView;

    QLineEdit*   m_searchEdit  {nullptr};
    QComboBox*   m_fieldCombo  {nullptr};
    QTreeWidget* m_tree        {nullptr};
    QPushButton* m_clearBtn    {nullptr};
    QLabel*      m_statusLabel {nullptr};

    /// Maps trace-ID key → event indices (top-level items).
    std::map<std::string, std::vector<unsigned long>> m_traceEvents;
};

} // namespace ui::qt
