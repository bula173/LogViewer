#include "SequenceDiagramPanel.hpp"

#include "EventsContainer.hpp"
#include "events/EventsTableView.hpp"
#include "Logger.hpp"
#include "panels/ActorDefinition.hpp"

#include <QFutureWatcher>
#include <QGraphicsLineItem>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QtConcurrent/QtConcurrentRun>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QHBoxLayout>
#include <QLabel>
#include <QPolygonF>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <climits>
#include <map>
#include <vector>

// ---------------------------------------------------------------------------
// ClickScene — defined at file scope so AUTOMOC can process Q_OBJECT
// ---------------------------------------------------------------------------

/// QGraphicsScene subclass that emits arrowClicked(eventIndex) when the
/// user double-clicks an ArrowItem.
class ClickScene : public QGraphicsScene
{
    Q_OBJECT
public:
    using QGraphicsScene::QGraphicsScene;

signals:
    void arrowClicked(qulonglong eventIndex);

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e) override;
};

// ---------------------------------------------------------------------------
// ArrowItem — clickable line that stores the originating event index
// ---------------------------------------------------------------------------

class ArrowItem : public QGraphicsLineItem
{
public:
    ArrowItem(qreal x1, qreal y1, qreal x2, qreal y2,
              qulonglong eventIdx, QGraphicsItem* parent = nullptr)
        : QGraphicsLineItem(x1, y1, x2, y2, parent)
        , m_eventIdx(eventIdx)
    {
        setAcceptHoverEvents(true);
        setFlag(QGraphicsItem::ItemIsSelectable);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QString("Event #%1 — double-click to navigate")
                       .arg(static_cast<qulonglong>(eventIdx)));
    }

    qulonglong eventIndex() const { return m_eventIdx; }

    // Expand the clickable shape to a 10 px band around the line so thin
    // arrows are easy to double-click even at small zoom levels.
    QPainterPath shape() const override
    {
        QPainterPath p;
        p.moveTo(line().p1());
        p.lineTo(line().p2());
        QPainterPathStroker stroker;
        stroker.setWidth(10.0);
        return stroker.createStroke(p);
    }

private:
    qulonglong m_eventIdx;
};

void ClickScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* e)
{
    const auto its = items(e->scenePos());
    for (auto* item : its)
    {
        if (auto* arrow = dynamic_cast<ArrowItem*>(item))
        {
            emit arrowClicked(arrow->eventIndex());
            return;
        }
    }
    QGraphicsScene::mouseDoubleClickEvent(e);
}

// ---------------------------------------------------------------------------
// Panel implementation
// ---------------------------------------------------------------------------

namespace ui::qt
{

namespace {
    constexpr int kActorBoxW  = 130;
    constexpr int kActorBoxH  = 28;
    constexpr int kColSpacing = 170;
    constexpr int kRowH       = 44;
    constexpr int kHeaderY    = 8;
    constexpr int kFirstMsgY  = kHeaderY + kActorBoxH + 16;
    constexpr int kArrowHead  = 8;
    constexpr int kLabelOffY  = -13;
}

SequenceDiagramPanel::SequenceDiagramPanel(db::EventsContainer& events,
                                           EventsTableView*     eventsView,
                                           QWidget*             parent)
    : QWidget(parent)
    , m_events(events)
    , m_eventsView(eventsView)
{
    BuildLayout();
}

void SequenceDiagramPanel::BuildLayout()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Toolbar ───────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout();

    m_refreshBtn = new QPushButton(tr("Re-discover"), this);
    m_refreshBtn->setToolTip(
        tr("Re-scan loaded events to detect from/to actor fields automatically"));
    toolbar->addWidget(m_refreshBtn);

    toolbar->addWidget(new QLabel(tr("Show first"), this));
    m_limitSpin = new QSpinBox(this);
    m_limitSpin->setRange(10, 5000);
    m_limitSpin->setValue(200);
    m_limitSpin->setSingleStep(50);
    m_limitSpin->setSuffix(tr(" messages"));
    toolbar->addWidget(m_limitSpin);

    toolbar->addStretch();

    m_statusLabel = new QLabel(tr("Load a log file to generate the diagram"), this);
    m_statusLabel->setStyleSheet("color: gray;");
    toolbar->addWidget(m_statusLabel, 1);

    root->addLayout(toolbar);

    // ── Graphics view ─────────────────────────────────────────────────────
    auto* scene = new ClickScene(this);
    m_scene = scene;
    m_scene->setBackgroundBrush(palette().base());

    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    root->addWidget(m_view, 1);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_refreshBtn, &QPushButton::clicked,
            this, &SequenceDiagramPanel::Refresh);
    connect(m_limitSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SequenceDiagramPanel::OnLimitChanged);
    connect(scene, &ClickScene::arrowClicked,
            this, &SequenceDiagramPanel::OnSceneClicked);
}

// ---------------------------------------------------------------------------
// Public slot
// ---------------------------------------------------------------------------

void SequenceDiagramPanel::Refresh()
{
    // Prevent re-entrant calls while discovery or rendering is in progress.
    if (m_watcher && m_watcher->isRunning()) return;

    m_scene->clear();
    m_pattern.reset();

    if (m_events.Size() == 0)
    {
        m_statusLabel->setText(tr("No events loaded"));
        m_statusLabel->setStyleSheet("color: gray;");
        return;
    }

    m_statusLabel->setText(tr("Discovering actors…"));
    m_statusLabel->setStyleSheet("color: orange;");
    if (m_refreshBtn) m_refreshBtn->setEnabled(false);

    // Run discovery on a background thread so the UI stays responsive.
    if (!m_watcher)
    {
        m_watcher = new QFutureWatcher<analyzer::ActorDiscoveryResult>(this);
        connect(m_watcher, &QFutureWatcher<analyzer::ActorDiscoveryResult>::finished,
                this, &SequenceDiagramPanel::OnDiscoveryFinished);
    }
    m_watcher->setFuture(
        QtConcurrent::run([this]() {
            return analyzer::ActorDiscoverer::Discover(m_events);
        }));
}

void SequenceDiagramPanel::SetDefinitions(const std::vector<ActorDefinition>& defs)
{
    m_selfActor.clear();
    m_aliasMap.clear();

    for (const auto& def : defs)
    {
        if (def.isSelf)
        {
            // Self is identified by the actor's name (for fixed-name defs)
            // or by the captured values (for capture defs). We store the
            // definition name; RenderDiagram matches it against the actor set.
            m_selfActor = def.name;
        }

        if (!def.useCaptures)
        {
            // Fixed-name definition: alias applies to the whole actor name.
            if (!def.alias.empty())
                m_aliasMap[def.name] = def.alias;
        }
        else
        {
            // Capture-group definition: per-value aliases from captureAliases.
            for (const auto& [raw, alias] : def.captureAliases)
                if (!alias.empty())
                    m_aliasMap[raw] = alias;
        }
    }

    // Re-render with updated aliases/self if a pattern is already known.
    if (m_pattern)
        RenderDiagram(*m_pattern);
}

void SequenceDiagramPanel::OnDiscoveryFinished()
{
    if (m_refreshBtn) m_refreshBtn->setEnabled(true);

    const auto result = m_watcher->result();
    if (result.patterns.empty())
    {
        m_statusLabel->setText(
            tr("No exchange pattern detected. "
               "Try a log with sender/receiver/source/dest or actor/direction fields."));
        m_statusLabel->setStyleSheet("color: gray;");
        return;
    }

    m_pattern = result.patterns[0];
    RenderDiagram(*m_pattern);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void SequenceDiagramPanel::RenderDiagram(const analyzer::ExchangePattern& pat)
{
    m_scene->clear();

    const int limit = m_limitSpin->value();

    // Build alias helper: returns display name for a raw actor value.
    auto displayName = [this](const std::string& raw) -> QString {
        const auto it = m_aliasMap.find(raw);
        return QString::fromStdString(it != m_aliasMap.end() ? it->second : raw);
    };

    // Ordered actor list — self actor placed first if present.
    std::vector<std::string> actors(pat.actors.begin(), pat.actors.end());
    std::sort(actors.begin(), actors.end());
    // Move self to front for visual prominence.
    if (!m_selfActor.empty())
    {
        auto selfIt = std::find(actors.begin(), actors.end(), m_selfActor);
        if (selfIt != actors.end())
            std::rotate(actors.begin(), selfIt, selfIt + 1);
    }
    std::map<std::string, int> col;
    for (int i = 0; i < static_cast<int>(actors.size()); ++i)
        col[actors[static_cast<size_t>(i)]] = i;

    const int   numActors = static_cast<int>(actors.size());
    const qreal actorCX   = kActorBoxW / 2.0;

    // ── Collect message rows ──────────────────────────────────────────────
    struct MsgRow { int fromCol, toCol; QString label; qulonglong idx; };
    std::vector<MsgRow> rows;
    rows.reserve(static_cast<size_t>(limit));

    for (size_t i = 0; i < m_events.Size() && static_cast<int>(rows.size()) < limit; ++i)
    {
        const auto& ev  = m_events.GetEvent(i);
        const auto  frm = ev.findByKey(pat.senderField);
        const auto  too = ev.findByKey(pat.receiverField);
        if (frm.empty() || too.empty()) continue;
        auto fIt = col.find(frm), tIt = col.find(too);
        if (fIt == col.end() || tIt == col.end()) continue;

        QString lbl = pat.labelField.empty()
                      ? QString{}
                      : QString::fromStdString(ev.findByKey(pat.labelField));
        if (lbl.size() > 32) { using namespace Qt::StringLiterals; lbl = lbl.left(30) + u"…"_s; }
        rows.push_back({fIt->second, tIt->second, std::move(lbl),
                        static_cast<qulonglong>(i)});
    }

    // ── Draw lifelines + actor boxes ──────────────────────────────────────
    const int diagramH = kFirstMsgY + static_cast<int>(rows.size()) * kRowH + 24;

    QPen lifePen(QColor(180, 180, 180));
    lifePen.setStyle(Qt::DashLine);

    for (int i = 0; i < numActors; ++i)
    {
        const qreal cx = i * kColSpacing + actorCX;

        const bool isSelf = (!m_selfActor.empty() && actors[static_cast<size_t>(i)] == m_selfActor);
        const QColor boxBorder = isSelf ? QColor(0, 120, 0)   : QColor(80, 100, 180);
        const QColor boxFill   = isSelf ? QColor(200, 240, 200): QColor(220, 225, 255);
        const QColor txtColor  = isSelf ? QColor(0, 80, 0)    : QColor(30, 30, 80);

        m_scene->addRect(cx - kActorBoxW / 2.0, kHeaderY, kActorBoxW, kActorBoxH,
                         QPen(boxBorder, isSelf ? 2 : 1), QBrush(boxFill));

        const QString label = displayName(actors[static_cast<size_t>(i)]);
        auto* txt = m_scene->addText(label);
        {
            QFont f = txt->font();
            f.setBold(true);
            f.setPointSize(8);
            txt->setFont(f);
        }
        txt->setDefaultTextColor(txtColor);
        txt->setPos(cx - txt->boundingRect().width() / 2.0,
                    kHeaderY + (kActorBoxH - txt->boundingRect().height()) / 2.0);

        m_scene->addLine(cx, kHeaderY + kActorBoxH, cx, diagramH, lifePen);
    }

    // ── Draw arrows ───────────────────────────────────────────────────────
    // Arrow colours depending on self-actor involvement.
    // Outgoing (from self) = green, incoming (to self) = blue,
    // self-loop = purple, peer-to-peer = gray.
    const bool hasSelf = !m_selfActor.empty() && col.count(m_selfActor);
    const int  selfCol = hasSelf ? col.at(m_selfActor) : -1;

    auto arrowColor = [&](int fromCol, int toCol) -> QColor {
        if (fromCol == toCol)               return QColor(120, 60, 160); // self-loop
        if (hasSelf && fromCol == selfCol)  return QColor(0, 140, 0);   // outgoing
        if (hasSelf && toCol   == selfCol)  return QColor(30, 80, 200); // incoming
        return QColor(100, 100, 100);                                    // peer-to-peer
    };

    for (int r = 0; r < static_cast<int>(rows.size()); ++r)
    {
        const MsgRow& mr = rows[static_cast<size_t>(r)];
        const qreal   y  = kFirstMsgY + r * kRowH;
        const qreal   x1 = mr.fromCol * kColSpacing + actorCX;
        const qreal   x2 = mr.toCol   * kColSpacing + actorCX;
        const QColor  clr = arrowColor(mr.fromCol, mr.toCol);
        QPen pen(clr);

        if (mr.fromCol == mr.toCol)
        {
            // Self-message loop
            const qreal lx = x1 + 14;
            m_scene->addLine(x1, y,      lx, y,      pen);
            m_scene->addLine(lx, y,      lx, y + 18, pen);
            auto* back = new ArrowItem(lx, y + 18, x1, y + 18, mr.idx);
            back->setPen(pen);
            m_scene->addItem(back);
            QPolygonF h;
            h << QPointF(x1, y+18)
              << QPointF(x1 + kArrowHead, y + 18 - kArrowHead/2.0)
              << QPointF(x1 + kArrowHead, y + 18 + kArrowHead/2.0);
            m_scene->addPolygon(h, pen, QBrush(clr));
        }
        else
        {
            auto* line = new ArrowItem(x1, y, x2, y, mr.idx);
            line->setPen(pen);
            m_scene->addItem(line);
            const bool rgt = (x2 > x1);
            QPolygonF h;
            if (rgt)
                h << QPointF(x2, y) << QPointF(x2-kArrowHead, y-kArrowHead/2.0)
                                     << QPointF(x2-kArrowHead, y+kArrowHead/2.0);
            else
                h << QPointF(x2, y) << QPointF(x2+kArrowHead, y-kArrowHead/2.0)
                                     << QPointF(x2+kArrowHead, y+kArrowHead/2.0);
            m_scene->addPolygon(h, pen, QBrush(clr));
        }

        if (!mr.label.isEmpty())
        {
            auto* lbl = m_scene->addText(mr.label);
            QFont lf = lbl->font(); lf.setPointSize(7); lbl->setFont(lf);
            lbl->setDefaultTextColor(clr);
            const qreal mx = (x1 + x2) / 2.0 - lbl->boundingRect().width() / 2.0;
            lbl->setPos(mx, y + kLabelOffY);
        }
    }

    m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-10, -10, 10, 10));
    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);

    const bool trunc = (static_cast<int>(rows.size()) == limit);
    m_statusLabel->setText(
        tr("Detected: %1 → %2%3  ·  %4 actor(s)  ·  %5 message(s)%6")
            .arg(QString::fromStdString(pat.senderField))
            .arg(QString::fromStdString(pat.receiverField))
            .arg(pat.labelField.empty() ? "" :
                 tr(" (label: %1)").arg(QString::fromStdString(pat.labelField)))
            .arg(numActors)
            .arg(rows.size())
            .arg(trunc ? tr(" (limit)") : ""));
    m_statusLabel->setStyleSheet("color: #226622;");
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void SequenceDiagramPanel::OnLimitChanged(int)
{
    if (m_pattern) RenderDiagram(*m_pattern);
}

void SequenceDiagramPanel::OnSceneClicked(qulonglong eventIndex)
{
    if (m_eventsView && eventIndex <= static_cast<qulonglong>(INT_MAX))
        m_eventsView->ScrollToActualRow(static_cast<int>(eventIndex));
}

} // namespace ui::qt

// AUTOMOC requires this at file scope (outside all namespaces)
#include "SequenceDiagramPanel.moc"
