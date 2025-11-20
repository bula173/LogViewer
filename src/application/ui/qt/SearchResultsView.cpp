#include "ui/qt/SearchResultsView.hpp"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QString>
#include <QTreeWidgetItem>

namespace ui::qt
{

SearchResultsView::SearchResultsView(QWidget* parent)
    : QTreeWidget(parent)
{
    setUniformRowHeights(true);
    setRootIsDecorated(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    header()->setStretchLastSection(true);
    connect(this, &QTreeWidget::itemActivated, this,
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
    clear();

    QStringList headers;
    headers << tr("Event ID") << tr("Matched Key") << tr("Match");
    for (const auto& column : m_columns)
    {
        headers << QString::fromStdString(column);
    }
    setColumnCount(headers.size());
    setHeaderLabels(headers);
}

void SearchResultsView::AppendResult(const mvc::SearchResultRow& row)
{
    auto* item = new QTreeWidgetItem();
    item->setText(kEventIdColumn, QString::number(row.eventId));
    item->setData(kEventIdColumn, Qt::UserRole, row.eventId);
    item->setText(kMatchedKeyColumn,
        QString::fromStdString(row.matchedKey));
    item->setText(kMatchTextColumn,
        QString::fromStdString(row.matchedText));

    const int dynamicOffset = kMatchTextColumn + 1;
    for (int i = 0; i < static_cast<int>(row.columnValues.size()); ++i)
    {
        item->setText(dynamicOffset + i,
            QString::fromStdString(row.columnValues.at(i)));
    }

    addTopLevelItem(item);
}

void SearchResultsView::EndUpdate()
{
    setUpdatesEnabled(true);
    update();
}

void SearchResultsView::Clear()
{
    m_columns.clear();
    QTreeWidget::clear();
}

void SearchResultsView::SimulateActivation(long eventId)
{
    if (m_observer)
        m_observer->OnSearchResultActivated(eventId);
}

void SearchResultsView::HandleActivation(QTreeWidgetItem* item, int)
{
    if (!item || !m_observer)
        return;

    const long eventId = item->data(kEventIdColumn, Qt::UserRole).toLongLong();
    m_observer->OnSearchResultActivated(eventId);
}

} // namespace ui::qt
