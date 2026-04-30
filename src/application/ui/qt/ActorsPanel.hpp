/**
 * @file ActorsPanel.hpp
 * @brief Panel showing per-actor event statistics and actor-based filtering.
 *
 * @details
 * Actors are extracted from log events using regexp-based @c ActorDefinition
 * rules.  The panel displays a tree where each top-level row represents one
 * definition and child rows represent the individual matched actors (in
 * capture-group mode).
 *
 * ### Interaction with filter profiles
 * The check state of every actor row can be saved and restored as part of a
 * @c FilterProfile via @c GetUncheckedActors() and @c RestoreUncheckedActors().
 * Actor keys use the @c ActorKey::Encode() / @c ActorKey::Decode() helpers
 * from ActorDefinition.hpp.
 *
 * @author LogViewer Development Team
 * @date 2026
 */
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

  public:
    /**
     * @brief Returns the set of actor keys for actors that are currently unchecked.
     *
     * Each element is an @c ActorKey::Encode(defName, actorName) string.
     * Called by MainWindow when saving a @c FilterProfile.
     *
     * @return Opaque set of unchecked-actor keys.
     */
    [[nodiscard]] std::set<std::string> GetUncheckedActors() const
    { return m_uncheckedActors; }

    /**
     * @brief Restores actor check state from a saved profile.
     *
     * Sets the internal unchecked-actors set to @p unchecked, walks the tree
     * widget (with signals blocked) to update individual check boxes, and
     * finally calls @c ApplyCheckedFilter() to refresh the events view.
     *
     * @param unchecked  Set of @c ActorKey::Encode(defName, actorName) keys
     *                   that should be in the unchecked state after the call.
     *                   Keys not in the set are checked.  An empty set checks
     *                   all actors (equivalent to clearing the actor filter).
     */
    void RestoreUncheckedActors(const std::set<std::string>& unchecked);

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
