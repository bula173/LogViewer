#pragma once

#include "ActorDefinition.hpp"

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
 * Two operating modes:
 *  - **Defined-actor mode**: when actor definitions (regexp) are supplied via
 *    SetDefinitions(), events are matched against those patterns and attributed
 *    to the first matching named actor.
 *  - **Auto-detect mode** (fallback): unique values of the selected field are
 *    treated as actors (original behaviour).
 *
 * Clicking a row instantly filters the events view to that actor's events.
 * "Clear Filter" resets to showing all visible events.
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
    void PopulateFieldCombo();
    void FilterByActor(const std::string& actorValue);
    void RefreshWithDefinitions(const std::vector<unsigned long>& vis);
    void RefreshAutoDetect(const std::vector<unsigned long>& vis);
    void PopulateActorTable(size_t totalVisible);

    /// Returns the indices of currently visible events.
    std::vector<unsigned long> VisibleIndices() const;

    db::EventsContainer& m_events;
    EventsTableView*      m_eventsView;

    QComboBox*    m_fieldCombo  {nullptr};
    QPushButton*  m_clearBtn    {nullptr};
    QTableWidget* m_table       {nullptr};
    QLabel*       m_statusLabel {nullptr};
    QLabel*       m_modeLabel   {nullptr};

    std::map<std::string, ActorData>  m_actorCache;   ///< actor value → aggregated data
    std::vector<ActorDefinition>      m_definitions;  ///< regexp-based actor definitions
};

} // namespace ui::qt
