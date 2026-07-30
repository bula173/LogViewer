#include "TypeFilterView.hpp"

#include "Logger.hpp"

#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPoint>
#include <QSignalBlocker>
#include <QString>
#include <QVBoxLayout>

#include <set>

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

    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QListWidget::customContextMenuRequested, this,
        [this](const QPoint& pos)
        {
            QMenu menu(this);
            QAction* selectAllAction =
                menu.addAction(tr("Select All"));
            QAction* deselectAllAction =
                menu.addAction(tr("Deselect All"));
            QAction* invertSelectionAction =
                menu.addAction(tr("Invert Selection"));

            QAction* chosen =
                menu.exec(m_listWidget->viewport()->mapToGlobal(pos));
            if (!chosen)
                return;

            if (chosen == selectAllAction)
                SelectAll();
            else if (chosen == deselectAllAction)
                DeselectAll();
            else if (chosen == invertSelectionAction)
                InvertSelection();
        });

    // When a checkbox state changes, notify the filter changed callback
    // This triggers immediate filter reapplication as user toggles types
    connect(m_listWidget, &QListWidget::itemChanged, this, [this](QListWidgetItem*) {
        NotifyChanged();
    });
}

void TypeFilterView::SetOnFilterChanged(std::function<void()> handler)
{
    m_onFilterChanged = std::move(handler);
}

void TypeFilterView::ReplaceTypes(
    const std::vector<std::string>& types, bool checkedByDefault)
{
    util::Logger::Debug("[TypeFilterView] ReplaceTypes: {} type(s), all checked={}", types.size(), checkedByDefault);
    QSignalBlocker blocker(m_listWidget);
    m_listWidget->clear();
    for (const auto& type : types)
    {
        auto* item = new QListWidgetItem(
            QString::fromStdString(type), m_listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(checkedByDefault ? Qt::Checked : Qt::Unchecked);
    }
}

void TypeFilterView::ShowControl(bool show)
{
    setVisible(show);
}

void TypeFilterView::SelectAll()
{
    util::Logger::Debug("[TypeFilterView] SelectAll: checking all {} type(s)", m_listWidget->count());
    SetAll(Qt::Checked);
}

void TypeFilterView::DeselectAll()
{
    util::Logger::Debug("[TypeFilterView] DeselectAll: unchecking all {} type(s)", m_listWidget->count());
    SetAll(Qt::Unchecked);
}

void TypeFilterView::InvertSelection()
{
    util::Logger::Debug("[TypeFilterView] InvertSelection: toggling {} type(s)", m_listWidget->count());
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
    // Notify after toggling all items (block prevents itemChanged from firing)
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
    // Notify after updating all items (block prevents itemChanged from firing)
    NotifyChanged();
}

void TypeFilterView::SetCheckedTypes(const std::vector<std::string>& types)
{
    if (types.empty())
    {
        util::Logger::Debug("[TypeFilterView] SetCheckedTypes: empty list — falling back to SelectAll");
        SelectAll();
        return;
    }
    util::Logger::Debug("[TypeFilterView] SetCheckedTypes: applying {} checked type(s) out of {} total",
                        types.size(), m_listWidget->count());
    const std::set<std::string> typeSet(types.begin(), types.end());
    QSignalBlocker blocker(m_listWidget);
    for (int i = 0; i < m_listWidget->count(); ++i)
    {
        if (auto* item = m_listWidget->item(i))
        {
            const bool checked = typeSet.count(item->text().toStdString()) > 0;
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
    }
}

} // namespace ui::qt
