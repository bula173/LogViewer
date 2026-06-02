#pragma once

#include "analyzers/ActorDiscoverer.hpp"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QSpinBox>
#include <QWidget>

#include <optional>

namespace db   { class EventsContainer; }
namespace ui::qt { class EventsTableView; }

namespace ui::qt
{

/// Sequence diagram panel.
///
/// On Refresh(), scans the loaded events with ActorDiscoverer to find
/// from/to fields automatically, then renders a swimlane sequence diagram
/// in a QGraphicsView.  Each arrow represents one message exchange event;
/// clicking an arrow navigates to that event in the events table.
///
/// The panel works on any log format — it never looks for hard-coded field
/// names; everything is inferred from the data.
class SequenceDiagramPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SequenceDiagramPanel(db::EventsContainer& events,
                                  EventsTableView*     eventsView,
                                  QWidget*             parent = nullptr);

public slots:
    void Refresh();

private slots:
    void OnLimitChanged(int value);
    void OnSceneClicked(unsigned long eventIndex);

private:
    void BuildLayout();
    void RenderDiagram(const analyzer::ExchangePattern& pat);

    db::EventsContainer& m_events;
    EventsTableView*     m_eventsView;

    QGraphicsScene*  m_scene       {nullptr};
    QGraphicsView*   m_view        {nullptr};
    QSpinBox*        m_limitSpin   {nullptr};
    QLabel*          m_statusLabel {nullptr};

    std::optional<analyzer::ExchangePattern> m_pattern;
};

} // namespace ui::qt
