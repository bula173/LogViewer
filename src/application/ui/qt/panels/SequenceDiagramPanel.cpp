#include "SequenceDiagramPanel.hpp"

#include "EventsContainer.hpp"
#include "events/EventsTableView.hpp"
#include "Logger.hpp"

#include <QApplication>
#include <QGraphicsLineItem>
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
    void arrowClicked(unsigned long eventIndex);

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
              unsigned long eventIdx, QGraphicsItem* parent = nullptr)
        : QGraphicsLineItem(x1, y1, x2, y2, parent)
        , m_eventIdx(eventIdx)
    {
        setAcceptHoverEvents(true);
        setFlag(QGraphicsItem::ItemIsSelectable);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QString("Event #%1 — double-click to navigate")
                       .arg(static_cast<qulonglong>(eventIdx)));
    }

    unsigned long eventIndex() const { return m_eventIdx; }

private:
    unsigned long m_eventIdx;
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

    auto* refreshBtn = new QPushButton(tr("Re-discover"), this);
    refreshBtn->setToolTip(
        tr("Re-scan loaded events to detect from/to actor fields automatically"));
    toolbar->addWidget(refreshBtn);

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
    connect(refreshBtn,  &QPushButton::clicked,
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
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const auto result = analyzer::ActorDiscoverer::Discover(m_events);

    if (!result.exchange)
    {
        m_statusLabel->setText(
            tr("No from→to pattern detected. "
               "Try a log with sender/receiver/source/dest fields."));
        m_statusLabel->setStyleSheet("color: gray;");
        return;
    }

    m_pattern = result.exchange;
    RenderDiagram(*m_pattern);
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void SequenceDiagramPanel::RenderDiagram(const analyzer::ExchangePattern& pat)
{
    m_scene->clear();

    const int limit = m_limitSpin->value();

    // Ordered actor list
    std::vector<std::string> actors(pat.actors.begin(), pat.actors.end());
    std::sort(actors.begin(), actors.end());
    std::map<std::string, int> col;
    for (int i = 0; i < static_cast<int>(actors.size()); ++i)
        col[actors[static_cast<size_t>(i)]] = i;

    const int   numActors = static_cast<int>(actors.size());
    const qreal actorCX   = kActorBoxW / 2.0;

    // ── Collect message rows ──────────────────────────────────────────────
    struct MsgRow { int fromCol, toCol; std::string label; unsigned long idx; };
    std::vector<MsgRow> rows;
    rows.reserve(static_cast<size_t>(limit));

    for (unsigned long i = 0; i < m_events.Size() && static_cast<int>(rows.size()) < limit; ++i)
    {
        const auto& ev  = m_events.GetEvent(i);
        const auto  frm = ev.findByKey(pat.senderField);
        const auto  too = ev.findByKey(pat.receiverField);
        if (frm.empty() || too.empty()) continue;
        auto fIt = col.find(frm), tIt = col.find(too);
        if (fIt == col.end() || tIt == col.end()) continue;

        std::string lbl = pat.labelField.empty() ? "" : ev.findByKey(pat.labelField);
        if (lbl.size() > 32) lbl = lbl.substr(0, 30) + "…";
        rows.push_back({fIt->second, tIt->second, std::move(lbl), i});
    }

    // ── Draw lifelines + actor boxes ──────────────────────────────────────
    const int diagramH = kFirstMsgY + static_cast<int>(rows.size()) * kRowH + 24;

    QPen lifePen(QColor(180, 180, 180));
    lifePen.setStyle(Qt::DashLine);

    for (int i = 0; i < numActors; ++i)
    {
        const qreal cx = i * kColSpacing + actorCX;

        m_scene->addRect(cx - kActorBoxW / 2.0, kHeaderY, kActorBoxW, kActorBoxH,
                         QPen(QColor(80, 100, 180)), QBrush(QColor(220, 225, 255)));

        auto* txt = m_scene->addText(
            QString::fromStdString(actors[static_cast<size_t>(i)]));
        {
            QFont f = txt->font();
            f.setBold(true);
            f.setPointSize(8);
            txt->setFont(f);
        }
        txt->setDefaultTextColor(QColor(30, 30, 80));
        txt->setPos(cx - txt->boundingRect().width() / 2.0,
                    kHeaderY + (kActorBoxH - txt->boundingRect().height()) / 2.0);

        m_scene->addLine(cx, kHeaderY + kActorBoxH, cx, diagramH, lifePen);
    }

    // ── Draw arrows ───────────────────────────────────────────────────────
    const QColor kArrowClr(60, 80, 160);
    const QColor kSelfClr (120, 80, 160);

    for (int r = 0; r < static_cast<int>(rows.size()); ++r)
    {
        const MsgRow& mr = rows[static_cast<size_t>(r)];
        const qreal   y  = kFirstMsgY + r * kRowH;
        const qreal   x1 = mr.fromCol * kColSpacing + actorCX;
        const qreal   x2 = mr.toCol   * kColSpacing + actorCX;
        const QColor& clr = (mr.fromCol == mr.toCol) ? kSelfClr : kArrowClr;
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

        if (!mr.label.empty())
        {
            auto* lbl = m_scene->addText(QString::fromStdString(mr.label));
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

void SequenceDiagramPanel::OnSceneClicked(unsigned long eventIndex)
{
    if (m_eventsView)
        m_eventsView->ScrollToActualRow(static_cast<int>(eventIndex));
}

} // namespace ui::qt

// AUTOMOC requires this at file scope (outside all namespaces)
#include "SequenceDiagramPanel.moc"
