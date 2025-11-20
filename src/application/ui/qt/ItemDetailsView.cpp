#include "ui/qt/ItemDetailsView.hpp"

#include "db/EventsContainer.hpp"
#include "db/LogEvent.hpp"

#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace ui::qt
{

ItemDetailsView::ItemDetailsView(db::EventsContainer& events, QWidget* parent)
    : QWidget(parent)
    , m_events(events)
    , m_details(new QTableWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_details);

    m_details->setColumnCount(2);
    QStringList headers;
    headers << "Key" << "Value";
    m_details->setHorizontalHeaderLabels(headers);
    m_details->horizontalHeader()->setStretchLastSection(true);
    m_details->verticalHeader()->setVisible(false);
    m_details->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_details->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_details->setSelectionMode(QAbstractItemView::SingleSelection);
    m_details->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_details->setWordWrap(true);
}

void ItemDetailsView::RefreshView()
{
    DisplayEvent(m_events.GetCurrentItemIndex());
}

void ItemDetailsView::ShowControl(bool show)
{
    setVisible(show);
}

void ItemDetailsView::OnActualRowChanged(int actualRow)
{
    DisplayEvent(actualRow);
}

void ItemDetailsView::DisplayEvent(int actualRow)
{
    if (!m_details)
        return;

    if (actualRow < 0 || actualRow >= static_cast<int>(m_events.Size()))
    {
        m_details->setRowCount(0);
        return;
    }

    const auto& event = m_events.GetItem(actualRow);
    const auto& items = event.getEventItems();
    m_details->setRowCount(static_cast<int>(items.size()));

    int row = 0;
    for (const auto& [key, value] : items)
    {
        auto* keyItem =
            new QTableWidgetItem(QString::fromStdString(key));
        auto* valueItem =
            new QTableWidgetItem(QString::fromStdString(value));

        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        keyItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        valueItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);

        m_details->setItem(row, 0, keyItem);
        m_details->setItem(row, 1, valueItem);
        ++row;
    }
    m_details->resizeColumnsToContents();
    m_details->resizeRowsToContents();
}

} // namespace ui::qt
