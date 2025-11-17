#include "ui/qt/EventsTableView.hpp"

#include "db/EventsContainer.hpp"
#include "ui/qt/EventsTableModel.hpp"

#include <QHeaderView>
#include <QItemSelectionModel>

namespace ui::qt
{

EventsTableView::EventsTableView(
    db::EventsContainer& events, QWidget* parent)
    : QTableView(parent)
    , m_events(events)
{
    m_model = new EventsTableModel(m_events, this);
    setModel(m_model);

    InitializeView();
    ConnectSelectionSignals();
}

void EventsTableView::InitializeView()
{
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setSectionsMovable(false);
    horizontalHeader()->setSectionsClickable(true);
    verticalHeader()->setVisible(false);
    setSortingEnabled(false);
    ResizeColumnsToConfiguration();
}

void EventsTableView::ConnectSelectionSignals()
{
    connect(selectionModel(), &QItemSelectionModel::currentRowChanged, this,
        [this](const QModelIndex& current, const QModelIndex&)
        {
            if (!current.isValid())
            {
                emit CurrentActualRowChanged(-1);
                return;
            }

            const int actualIndex = m_model->ResolveToActualIndex(current.row());
            if (actualIndex >= 0)
                m_events.SetCurrentItem(actualIndex);

            emit CurrentActualRowChanged(actualIndex);
        });
}

void EventsTableView::ResizeColumnsToConfiguration()
{
    const auto widths = m_model->ColumnWidths();
    for (int column = 0; column < static_cast<int>(widths.size()); ++column)
    {
        const auto width = widths[static_cast<std::size_t>(column)];
        horizontalHeader()->resizeSection(column, width);
    }
}

void EventsTableView::RefreshColumns()
{
    if (!m_model)
        return;

    m_model->RefreshColumns();
    ResizeColumnsToConfiguration();
}

void EventsTableView::RefreshView()
{
    if (!m_model)
        return;

    m_model->SyncWithContainer();
    viewport()->update();
}

void EventsTableView::SetFilteredEvents(
    const std::vector<unsigned long>& filteredIndices)
{
    if (!m_model)
        return;

    m_model->SetFilteredIndices(filteredIndices);
    viewport()->update();
}

void EventsTableView::UpdateColors()
{
    if (!m_model)
        return;

    m_model->UpdateColors();
    viewport()->update();
}

int EventsTableView::CurrentActualRow() const
{
    if (!selectionModel())
        return -1;

    const QModelIndex current = selectionModel()->currentIndex();
    if (!current.isValid())
        return -1;

    return m_model->ResolveToActualIndex(current.row());
}

void EventsTableView::ScrollToActualRow(int actualRow)
{
    if (actualRow < 0 || !m_model)
        return;

    const int viewRow = m_model->RowFromActualIndex(actualRow);
    if (viewRow < 0)
        return;

    const QModelIndex idx = m_model->index(viewRow, 0);
    if (!idx.isValid())
        return;

    scrollTo(idx, QAbstractItemView::PositionAtCenter);
    if (selectionModel())
    {
        selectionModel()->setCurrentIndex(
            idx, QItemSelectionModel::ClearAndSelect);
    }
}

} // namespace ui::qt
