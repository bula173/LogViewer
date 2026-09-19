#pragma once

#include <QHeaderView>

#include <functional>

namespace ui::qt
{

/// Horizontal header that draws a small filter button at the right edge of
/// every section (highlighted while that column has a value filter).
/// Clicking the button emits FilterIndicatorClicked(); clicking anywhere else
/// on the section keeps its normal behaviour (sort, drag, resize).
class FilterHeaderView : public QHeaderView
{
    Q_OBJECT

  public:
    explicit FilterHeaderView(Qt::Orientation orientation, QWidget* parent = nullptr);

    /// Tells the header which columns currently carry a filter.
    void SetFilteredPredicate(std::function<bool(int)> isFiltered);

    /// Hit area of the filter button of @p logicalIndex, in viewport coordinates.
    QRect IndicatorRect(int logicalIndex) const;

  signals:
    void FilterIndicatorClicked(int logicalIndex);

  protected:
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

  private:
    std::function<bool(int)> m_isFiltered;
    bool m_swallowRelease {false};
};

} // namespace ui::qt
