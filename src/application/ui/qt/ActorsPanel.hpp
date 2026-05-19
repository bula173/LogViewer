#pragma once

#include "ActorDefinition.hpp"

#include <QLabel>
#include <QPushButton>
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
 * An actor is any field value (e.g. user, IP, process ID) that can be used to
 * identify who performed an action. The panel organises actors by their
 * ActorDefinition, showing hierarchical data with counts, types, and timestamps.
 *
 * Clicking a checkbox on a row filters the events view to show only events
 * from the selected actors. Multiple actors can be selected.
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

    /// Set actor definitions and refresh the tree.
    void SetDefinitions(const std::vector<ActorDefinition>& defs);

    /// Restore the set of unchecked actors from a saved filter.
    void RestoreUncheckedActors(const std::set<std::string>& unchecked);

  public:
    /// Get the set of currently unchecked actors (for filter persistence).
    [[nodiscard]] std::set<std::string> GetUncheckedActors() const
    {
        return m_uncheckedActors;
    }

  signals:
    /// Emitted when the user changes the actor direction in a context menu.
    /// Arguments: (definition name, actor name, isSubActor, new target actor name)
    void ActorDirectionChanged(const QString& defName, const QString& actorName,
                               bool isSubActor, const QString& newTarget);

  public:
    /// Per-actor statistics accumulated during a Refresh pass.
    struct ActorData
    {
        std::vector<unsigned long> indices;
        std::string                firstSeen;
        std::string                lastSeen;
        size_t                     errorCount {0};
        std::set<std::string>      types;
    };

  private:
    struct GroupedActorData
    {
        std::map<std::string, ActorData> actors;
        bool                             useCaptures {false};
    };

    /// Alias used in the sequence-diagram lambda capture.
    using GroupData = GroupedActorData;

    void BuildLayout();
    void RefreshWithDefinitions(const std::vector<unsigned long>& vis);
    void PopulateActorTree(size_t totalVisible);
    void ApplyCheckedFilter();
    void ShowActorContextMenu(const QPoint& pos);
    void ShowSequenceDiagram();
    [[nodiscard]] std::vector<unsigned long> VisibleIndices() const;

    db::EventsContainer& m_events;
    EventsTableView*      m_eventsView;

    QTreeWidget*  m_tree          {nullptr};
    QLabel*       m_statusLabel   {nullptr};
    QPushButton*  m_seqDiagBtn    {nullptr};

    std::vector<ActorDefinition>                   m_definitions;
    std::map<std::string, GroupedActorData>        m_groupedCache;
    std::set<std::string>                          m_uncheckedActors;
    bool                                           m_ignoreNextRefresh {false};
};

} // namespace ui::qt
