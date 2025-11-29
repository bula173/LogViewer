#include "ui/qt/ItemDetailsView.hpp"

#include "db/EventsContainer.hpp"
#include "db/LogEvent.hpp"

#include <QAction>
#include <QClipboard>
#include <QHeaderView>
#include <QGuiApplication>
#include <QKeySequence>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QTableWidgetItem>

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
    m_details->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_details->setSelectionMode(QAbstractItemView::SingleSelection);
    m_details->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_details->setWordWrap(true);
    m_details->setContextMenuPolicy(Qt::ActionsContextMenu);

    auto* copyAction = new QAction(tr("Copy"), m_details);
    copyAction->setShortcut(QKeySequence::Copy);
    m_details->addAction(copyAction);

    connect(copyAction, &QAction::triggered, this, [this]() {
        const auto items = m_details->selectedItems();
        if (items.isEmpty())
            return;

        QStringList lines;
        for (auto* item : items)
            lines << item->text();

        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(lines.join('\n'));
    });

    m_events.RegisterOndDataUpdated(this);
}

void ItemDetailsView::RefreshView()
{
    DisplayEvent(m_events.GetCurrentItemIndex());
}

void ItemDetailsView::ShowControl(bool show)
{
    setVisible(show);
}

void ItemDetailsView::OnDataUpdated()
{
    RefreshView();
}

void ItemDetailsView::OnCurrentIndexUpdated(const int index)
{
    DisplayEvent(index);
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
