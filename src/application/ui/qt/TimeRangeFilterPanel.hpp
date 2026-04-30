/**
 * @file TimeRangeFilterPanel.hpp
 * @brief Panel for filtering events by a user-defined timestamp range.
 *
 * @details
 * The panel occupies a tab in the Filters dock.  The user selects a timestamp
 * field (or types a custom one), optionally fills in ISO-format from/to bounds,
 * and clicks **Apply**.  The filter is evaluated by lexicographic string
 * comparison — correct for any timestamp representation that sorts
 * lexicographically (ISO 8601, epoch strings, etc.).
 *
 * The complete state can be serialised into a @c TimeRangeFilterPanel::State
 * value and round-tripped through @c FilterProfilesPanel for save/restore.
 *
 * @par Typical connections wired by MainWindow
 * @code
 * connect(m_timeRangePanel, &TimeRangeFilterPanel::FilterApplied,
 *         m_actorsPanel, &ActorsPanel::Refresh);
 * connect(m_timeRangePanel, &TimeRangeFilterPanel::FilterCleared,
 *         m_actorsPanel, &ActorsPanel::Refresh);
 * @endcode
 *
 * @author LogViewer Development Team
 * @date 2026
 */
#pragma once

#include <QWidget>
#include <string>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace db {
class EventsContainer;
}

namespace ui::qt {
class EventsTableView;
}

namespace ui::qt {

/**
 * @brief Panel for filtering events by a timestamp range.
 *
 * The user selects a timestamp field name, then specifies optional
 * "from" and "to" bounds (ISO-format strings).  The filter is applied
 * by string comparison, so any lexicographically-ordered timestamp
 * format (ISO 8601 etc.) works correctly.
 *
 * Applying the filter calls EventsTableView::SetFilteredEvents() directly
 * and emits FilterApplied() so that dependent panels (e.g. ActorsPanel)
 * can refresh against the new visible set.
 */
class TimeRangeFilterPanel : public QWidget
{
    Q_OBJECT

public:
    /// Plain-data snapshot of the panel state — used by filter profiles.
    struct State
    {
        std::string field {"timestamp"};
        std::string from;   ///< inclusive lower bound, empty = no lower bound
        std::string to;     ///< inclusive upper bound, empty = no upper bound
        bool        active {false};
    };

    explicit TimeRangeFilterPanel(db::EventsContainer& events,
                                  EventsTableView*     eventsView,
                                  QWidget*             parent = nullptr);

    /// Returns the current widget state (field, from, to, active).
    [[nodiscard]] State GetState() const;

    /// Restores state from a profile.  Does NOT automatically apply the filter.
    void SetState(const State& s);

    /// Applies the filter to the events view immediately.
    void Apply();

    /// Clears the time-range filter (shows all events).
    void Clear();

signals:
    /// Emitted after the filter has been applied to the events view.
    void FilterApplied();

    /// Emitted after the filter has been cleared.
    void FilterCleared();

private slots:
    void HandleAutoDetect();
    void HandleApply();
    void HandleClear();

private:
    void BuildLayout();

    /// Scans all events and returns indices whose timestamp falls in [from, to].
    [[nodiscard]] std::vector<unsigned long> ComputeMatchingIndices() const;

    db::EventsContainer& m_events;
    EventsTableView*     m_eventsView;

    QComboBox* m_fieldCombo   {nullptr};
    QLineEdit* m_fromEdit     {nullptr};
    QLineEdit* m_toEdit       {nullptr};
    QLabel*    m_statusLabel  {nullptr};
    bool       m_active       {false};
};

} // namespace ui::qt
