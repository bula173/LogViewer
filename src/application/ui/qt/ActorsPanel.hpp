#pragma once

#include "ActorDefinition.hpp"

#include <QLabel>
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
 * Actors are derived exclusively from the regexp-based definitions supplied via
 * SetDefinitions(). When no definitions are active the table is empty.
 *
 * Clicking a row filters the events view to that actor’s events.
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

    /// Update actor definitions; triggers an immediate Refresh().
    void SetDefinitions(const std::vector<ActorDefinition>& defs);

  public:
    struct ActorData
    {
        std::vector<unsigned long> indices;
        std::string                firstSeen;
        std::string                lastSeen;
        size_t                     errorCount {0};
        std::set<std::string>      types;
    };

  private:
    void BuildLayout();
    void FilterByActor(const std::string& actorValue);
    void RefreshWithDefinitions(const std::vector<unsigned long>& vis);
    void PopulateActorTable(size_t totalVisible);

    /// Returns the indices of currently visible events.
    std::vector<unsigned long> VisibleIndices() const;

    db::EventsContainer& m_events;
    EventsTableView*      m_eventsView;

    QTableWidget* m_table       {nullptr};
    QLabel*       m_statusLabel {nullptr};

    std::map<std::string, ActorData>  m_actorCache;   ///< actor value → aggregated data
    std::vector<ActorDefinition>      m_definitions;  ///< regexp-based actor definitions
};

} // namespace ui::qt
