#include "ui/qt/TypeFilterView.hpp"

#include <QListWidget>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QString>
#include <QVBoxLayout>

namespace ui::qt
{

TypeFilterView::TypeFilterView(QWidget* parent)
    : QWidget(parent)
    , m_listWidget(new QListWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_listWidget);

    m_listWidget->setSelectionMode(QAbstractItemView::NoSelection);

    connect(m_listWidget, &QListWidget::itemChanged, this,
        [this](QListWidgetItem*) { NotifyChanged(); });
}

void TypeFilterView::SetOnFilterChanged(std::function<void()> handler)
{
    m_onFilterChanged = std::move(handler);
}

void TypeFilterView::ReplaceTypes(
    const std::vector<std::string>& types, bool checkedByDefault)
{
    QSignalBlocker blocker(m_listWidget);
    m_listWidget->clear();
    for (const auto& type : types)
    {
        auto* item = new QListWidgetItem(
            QString::fromStdString(type), m_listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(checkedByDefault ? Qt::Checked : Qt::Unchecked);
    }
    NotifyChanged();
}

void TypeFilterView::ShowControl(bool show)
{
    setVisible(show);
}

void TypeFilterView::SelectAll()
{
    SetAll(Qt::Checked);
}

void TypeFilterView::DeselectAll()
{
    SetAll(Qt::Unchecked);
}

void TypeFilterView::InvertSelection()
{
    QSignalBlocker blocker(m_listWidget);
    for (int i = 0; i < m_listWidget->count(); ++i)
    {
        if (auto* item = m_listWidget->item(i))
        {
            const auto state =
                item->checkState() == Qt::Checked ? Qt::Unchecked
                                                   : Qt::Checked;
            item->setCheckState(state);
        }
    }
    NotifyChanged();
}

std::vector<std::string> TypeFilterView::CheckedTypes() const
{
    std::vector<std::string> selected;
    selected.reserve(static_cast<size_t>(m_listWidget->count()));
    for (int idx = 0; idx < m_listWidget->count(); ++idx)
    {
        const auto* item = m_listWidget->item(idx);
        if (item && item->checkState() == Qt::Checked)
            selected.emplace_back(item->text().toStdString());
    }
    return selected;
}

bool TypeFilterView::Empty() const
{
    return m_listWidget->count() == 0;
}

void TypeFilterView::NotifyChanged()
{
    if (m_onFilterChanged)
        m_onFilterChanged();
}

void TypeFilterView::SetAll(Qt::CheckState state)
{
    QSignalBlocker blocker(m_listWidget);
    for (int i = 0; i < m_listWidget->count(); ++i)
    {
        if (auto* item = m_listWidget->item(i))
            item->setCheckState(state);
    }
    NotifyChanged();
}

} // namespace ui::qt
