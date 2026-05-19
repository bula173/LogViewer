#pragma once

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

#include <map>
#include <set>
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
 * @brief Panel showing aggregate statistics per "actor" in the visible event set.
 *
 * An actor is any field value (e.g. user, IP, process ID) that can be used to
 * identify who performed an action. The field is configurable via a combo box
 * and persisted in Config::actorField.
 *
 * Clicking a row instantly filters the events view to that actor's events.
 * "Clear Filter" resets to showing all visible events.
 *
 * Refreshes automatically via a modelReset connection wired up in MainWindow.
 */
class ActorsPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit ActorsPanel(db::EventsContainer& events,
                         EventsTableView*     eventsView,
                         QWidget*             parent = nullptr);

  public slots:
    /// Recompute actor statistics from the current visible row set.
    void Refresh();

  private:
    struct ActorData
    {
        std::vector<unsigned long> indices;
        std::string                firstSeen;
        std::string                lastSeen;
        size_t                     errorCount {0};
        std::set<std::string>      types;
    };

    void BuildLayout();
    void PopulateFieldCombo();
    void FilterByActor(const std::string& actorValue);

    /// Returns the indices of currently visible events.
    std::vector<unsigned long> VisibleIndices() const;

    db::EventsContainer& m_events;
    EventsTableView*      m_eventsView;

    QComboBox*    m_fieldCombo  {nullptr};
    QPushButton*  m_clearBtn    {nullptr};
    QTableWidget* m_table       {nullptr};
    QLabel*       m_statusLabel {nullptr};

    std::map<std::string, ActorData> m_actorCache; ///< actor value → aggregated data
};

} // namespace ui::qt
