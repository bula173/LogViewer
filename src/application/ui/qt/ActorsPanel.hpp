#pragma once

#include "ActorDefinition.hpp"

#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTreeWidget>
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

  signals:
    /// Emitted when the user sets or clears a directed-to relationship via the
    /// context menu. isSubActor=true → update subActorDirectedTo[actorName];
    /// isSubActor=false → update definition-level directedTo.
    /// An empty target means "clear".
    void ActorDirectionChanged(const QString& defName,
                               const QString& actorName,
                               bool           isSubActor,
                               const QString& target);

  public:
    struct ActorData
    {
        std::vector<unsigned long> indices;
        std::string                firstSeen;
        std::string                lastSeen;
        size_t                     errorCount {0};
        std::set<std::string>      types;
    };

    /// Groups actors produced by one ActorDefinition.
    struct GroupData
    {
        bool                             useCaptures {false};
        std::map<std::string, ActorData> actors; ///< actor name → aggregated data
    };

  private:
    void BuildLayout();
    void ApplyCheckedFilter();
    void RefreshWithDefinitions(const std::vector<unsigned long>& vis);
    void PopulateActorTree(size_t totalVisible);
    void ShowSequenceDiagram();
    void ShowActorContextMenu(const QPoint& pos);

    /// Returns the indices of currently visible events.
    std::vector<unsigned long> VisibleIndices() const;

    db::EventsContainer& m_events;
    EventsTableView*      m_eventsView;

    QTreeWidget* m_tree        {nullptr};
    QLabel*      m_statusLabel {nullptr};
    QPushButton* m_seqDiagBtn  {nullptr};

    std::map<std::string, GroupData> m_groupedCache;   ///< def name → grouped actor data
    std::vector<ActorDefinition>     m_definitions;    ///< regexp-based actor definitions
    std::set<std::string>            m_uncheckedActors; ///< "defName\0actorName" keys
    bool                             m_ignoreNextRefresh {false}; ///< suppress rebuild after actor filter
};

} // namespace ui::qt
