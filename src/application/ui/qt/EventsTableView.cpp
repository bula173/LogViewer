#include "ui/qt/EventsTableView.hpp"

#include "application/util/Logger.hpp"
#include "db/EventsContainer.hpp"
#include "ui/qt/EventsTableModel.hpp"

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QKeySequence>
#include <QTableWidget>
#include <QVBoxLayout>

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
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionsMovable(true);
    horizontalHeader()->setSectionsClickable(true);
    verticalHeader()->setVisible(false);
    setSortingEnabled(false);
    ResizeColumnsToConfiguration();

    auto* copyAction = new QAction(tr("Copy"), this);
    copyAction->setShortcut(QKeySequence::Copy);
    addAction(copyAction);

    connect(copyAction, &QAction::triggered, this, [this]() {
        QModelIndexList indexes = selectionModel()
                                      ? selectionModel()->selectedIndexes()
                                      : QModelIndexList {};
        if (indexes.isEmpty())
            return;

        // Sort by row/column
        std::sort(indexes.begin(), indexes.end(),
            [](const QModelIndex& a, const QModelIndex& b) {
                if (a.row() == b.row())
                    return a.column() < b.column();
                return a.row() < b.row();
            });

        QStringList lines;
        int currentRow = indexes.first().row();
        QStringList rowValues;

        for (const auto& idx : indexes)
        {
            if (idx.row() != currentRow)
            {
                lines << rowValues.join('\t');
                rowValues.clear();
                currentRow = idx.row();
            }
            rowValues << model()->data(idx, Qt::DisplayRole).toString();
        }
        if (!rowValues.isEmpty())
            lines << rowValues.join('\t');

        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(lines.join('\n'));
    });

    m_events.RegisterOndDataUpdated(this);
}

void EventsTableView::ConnectSelectionSignals()
{
    connect(selectionModel(), &QItemSelectionModel::currentRowChanged, this,
        [this](const QModelIndex& current, const QModelIndex&)
        {
            if (!current.isValid())
            {
                util::Logger::Debug("[EventsTableView] currentRowChanged: invalid -> -1");
                emit CurrentActualRowChanged(-1);
                return;
            }

            const int actualIndex = m_model->ResolveToActualIndex(current.row());
            util::Logger::Debug(
                "[EventsTableView] currentRowChanged viewRow={} actualIndex={}",
                current.row(), actualIndex);

            emit CurrentActualRowChanged(actualIndex);
            if (actualIndex >= 0)
                m_events.SetCurrentItem(actualIndex);

           
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
    
    // Force header to recalculate stretch section
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setStretchLastSection(true);
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

const std::vector<unsigned long>* EventsTableView::GetFilteredIndices() const
{
    if (!m_model)
        return nullptr;
    
    const auto& indices = m_model->GetFilteredIndices();
    return indices.empty() ? nullptr : &indices;
}

void EventsTableView::UpdateColors()
{
    if (!m_model)
        return;

    m_model->UpdateColors();
    viewport()->update();
}

void EventsTableView::OnDataUpdated()
{
    RefreshView();
}

void EventsTableView::OnCurrentIndexUpdated(const int index)
{
    util::Logger::Debug("[EventsTableView] OnCurrentIndexUpdated index={}", index);

    if (index < 0)
    {
        if (selectionModel())
            selectionModel()->clearSelection();
        return;
    }    

    // Ensure the view scrolls and selects the corresponding row
    ScrollToActualRow(index);
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
    util::Logger::Debug("[EventsTableView] ScrollToActualRow actualRow={}", actualRow);

    if (actualRow < 0 || !m_model)
        return;

    const int viewRow = m_model->RowFromActualIndex(actualRow);
    util::Logger::Debug("[EventsTableView] mapped actualRow={} to viewRow={}",
        actualRow, viewRow);
    if (viewRow < 0)
        return;

    const QModelIndex idx = m_model->index(viewRow, 0);
    if (!idx.isValid())
    {
        util::Logger::Warn("[EventsTableView] index invalid for viewRow={}", viewRow);
        return;
    }

    // Only scroll if the row is outside the visible area
    const QRect visibleRect = viewport()->rect();
    const QModelIndex topIndex = indexAt(visibleRect.topLeft());
    const QModelIndex bottomIndex = indexAt(visibleRect.bottomLeft());
    const int topRow = topIndex.isValid() ? topIndex.row() : -1;
    const int bottomRow = bottomIndex.isValid() ? bottomIndex.row() : -1;

    if (topRow == -1 || bottomRow == -1 || viewRow < topRow || viewRow > bottomRow)
    {
        if (!hasFocus())
            setFocus(Qt::OtherFocusReason);

        scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }

    if (selectionModel())
    {
        selectionModel()->setCurrentIndex(
            idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        util::Logger::Debug("[EventsTableView] selection set to viewRow={}", viewRow);
    }

}

} // namespace ui::qt
