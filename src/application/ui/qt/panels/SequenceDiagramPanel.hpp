#pragma once

#include "analyzers/ActorDiscoverer.hpp"
#include "panels/ActorDefinition.hpp"

#include <QFutureWatcher>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

#include <map>
#include <optional>
#include <string>

namespace db   { class EventsContainer; }
namespace ui::qt { class EventsTableView; }

namespace ui::qt
{

/// Sequence diagram panel.
///
/// On Refresh(), runs ActorDiscoverer::Discover on a background thread
/// (QtConcurrent) so the UI stays responsive.  Once discovery completes
/// the exchange pattern is stored and RenderDiagram() draws the swimlane.
/// Each arrow is clickable (double-click) to navigate to the source event.
class SequenceDiagramPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SequenceDiagramPanel(db::EventsContainer& events,
                                  EventsTableView*     eventsView,
                                  QWidget*             parent = nullptr);

public slots:
    void Refresh();
    /// Called whenever actor definitions change.  Updates alias and self
    /// information used during rendering without re-running discovery.
    void SetDefinitions(const std::vector<ActorDefinition>& defs);

private slots:
    void OnDiscoveryFinished();
    void OnLimitChanged(int value);
    void OnSceneClicked(qulonglong eventIndex);

private:
    void BuildLayout();
    void RenderDiagram(const analyzer::ExchangePattern& pat);

    db::EventsContainer& m_events;
    EventsTableView*     m_eventsView;

    QGraphicsScene*  m_scene       {nullptr};
    QGraphicsView*   m_view        {nullptr};
    QSpinBox*        m_limitSpin   {nullptr};
    QLabel*          m_statusLabel {nullptr};
    QPushButton*     m_refreshBtn  {nullptr};

    std::optional<analyzer::ExchangePattern>        m_pattern;
    QFutureWatcher<analyzer::ActorDiscoveryResult>* m_watcher {nullptr};

    // Derived from actor definitions; rebuilt on SetDefinitions().
    std::string                        m_selfActor;   ///< Raw value of the "self" actor (empty = none)
    std::map<std::string, std::string> m_aliasMap;    ///< raw value → display alias
};

} // namespace ui::qt
