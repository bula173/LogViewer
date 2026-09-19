#include "FilterHeaderView.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

namespace ui::qt
{

namespace {
constexpr int kButtonW = 16;
constexpr int kButtonH = 14;
constexpr int kMargin  = 3;
}

FilterHeaderView::FilterHeaderView(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView(orientation, parent)
{
}

void FilterHeaderView::SetFilteredPredicate(std::function<bool(int)> isFiltered)
{
    m_isFiltered = std::move(isFiltered);
    viewport()->update();
}

QRect FilterHeaderView::IndicatorRect(int logicalIndex) const
{
    const int left  = sectionViewportPosition(logicalIndex);
    const int right = left + sectionSize(logicalIndex);
    return QRect(right - kButtonW - kMargin, (height() - kButtonH) / 2, kButtonW, kButtonH);
}

void FilterHeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const
{
    QHeaderView::paintSection(painter, rect, logicalIndex);
    if (!rect.isValid() || rect.width() < 2 * kButtonW)
        return;

    const bool filtered = m_isFiltered && m_isFiltered(logicalIndex);
    const QRect r(rect.right() - kButtonW - kMargin, rect.top() + (rect.height() - kButtonH) / 2,
                  kButtonW, kButtonH);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);

    if (filtered)
    {
        // Funnel
        painter->setBrush(palette().color(QPalette::Highlight));
        const qreal x = r.x() + 2, y = r.y() + 1, w = r.width() - 4, h = r.height() - 2;
        painter->drawPolygon(QPolygonF({
            {x, y}, {x + w, y}, {x + w * 0.62, y + h * 0.5},
            {x + w * 0.62, y + h}, {x + w * 0.38, y + h * 0.8}, {x + w * 0.38, y + h * 0.5}}));
    }
    else
    {
        // Drop-down triangle
        QColor c = palette().color(QPalette::ButtonText);
        c.setAlpha(150);
        painter->setBrush(c);
        const qreal cx = r.center().x() + 0.5, cy = r.center().y() + 0.5;
        painter->drawPolygon(QPolygonF({{cx - 4, cy - 2}, {cx + 4, cy - 2}, {cx, cy + 3}}));
    }
    painter->restore();
}

void FilterHeaderView::mousePressEvent(QMouseEvent* event)
{
    m_swallowRelease = false;
    if (event->button() == Qt::LeftButton)
    {
        const int idx = logicalIndexAt(event->position().toPoint());
        if (idx >= 0 && sectionSize(idx) >= 2 * kButtonW &&
            IndicatorRect(idx).contains(event->position().toPoint()))
        {
            // Own the click: no sort, no section drag.
            m_swallowRelease = true;
            event->accept();
            emit FilterIndicatorClicked(idx);
            return;
        }
    }
    QHeaderView::mousePressEvent(event);
}

void FilterHeaderView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_swallowRelease)
    {
        m_swallowRelease = false;
        event->accept();
        return;
    }
    QHeaderView::mouseReleaseEvent(event);
}

} // namespace ui::qt
