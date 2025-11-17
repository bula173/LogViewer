#include "ui/qt/SearchResultsView.hpp"

#include <QListWidgetItem>
#include <QString>

namespace ui::qt
{

SearchResultsView::SearchResultsView(QWidget* parent)
    : QListWidget(parent)
{
    connect(this, &QListWidget::itemActivated, this,
        &SearchResultsView::HandleActivation);
}

void SearchResultsView::SetObserver(ui::ISearchResultsViewObserver* observer)
{
    m_observer = observer;
}

void SearchResultsView::BeginUpdate(const std::vector<std::string>& columns)
{
    m_columns = columns;
    setUpdatesEnabled(false);
    QListWidget::clear();
}

void SearchResultsView::AppendResult(const mvc::SearchResultRow& row)
{
    auto* item = new QListWidgetItem(
        QString::fromStdString(row.matchedText), this);
    item->setData(Qt::UserRole, row.eventId);
    addItem(item);
}

void SearchResultsView::EndUpdate()
{
    setUpdatesEnabled(true);
    update();
}

void SearchResultsView::Clear()
{
    m_columns.clear();
    QListWidget::clear();
}

void SearchResultsView::SimulateActivation(long eventId)
{
    if (m_observer)
        m_observer->OnSearchResultActivated(eventId);
}

void SearchResultsView::HandleActivation(QListWidgetItem* item)
{
    if (!item || !m_observer)
        return;

    const long eventId = item->data(Qt::UserRole).toLongLong();
    m_observer->OnSearchResultActivated(eventId);
}

} // namespace ui::qt
