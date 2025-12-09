#include "ui/qt/FiltersPanel.hpp"

#include "ui/qt/FilterEditorDialog.hpp"
#include "util/Logger.hpp"

#include <QColor>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <initializer_list>
#include <memory>

namespace ui::qt
{
namespace
{
constexpr int kEnabledColumn = 0;
constexpr int kNameColumn = 1;
constexpr int kTargetColumn = 2;
constexpr int kPatternColumn = 3;
}

FiltersPanel::FiltersPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildUi();
    RefreshFilters();
}

void FiltersPanel::BuildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    QStringList headers;
    headers << tr("Enabled") << tr("Name") << tr("Target") << tr("Pattern");
    m_table->setHorizontalHeaderLabels(headers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    auto makeButtonRow = [this](std::initializer_list<QPushButton**> buttons,
                                std::initializer_list<QString> labels) {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        auto labelIt = labels.begin();
        for (auto* btnPtr : buttons)
        {
            auto* button = new QPushButton(*(labelIt++), this);
            row->addWidget(button);
            *btnPtr = button;
        }
        return row;
    };

    auto* row1 = makeButtonRow({&m_addButton, &m_editButton, &m_removeButton},
        {tr("Add"), tr("Edit"), tr("Remove")});
    layout->addLayout(row1);

    auto* row2 = makeButtonRow({&m_applyButton, &m_clearButton},
        {tr("Apply Filters"), tr("Clear All")});
    layout->addLayout(row2);

    auto* row3 = makeButtonRow({&m_saveAsButton, &m_loadButton},
        {tr("Save Filters As..."), tr("Load Filters...")});
    layout->addLayout(row3);

    m_editButton->setEnabled(false);
    m_removeButton->setEnabled(false);

    connect(m_addButton, &QPushButton::clicked, this, &FiltersPanel::HandleAdd);
    connect(m_editButton, &QPushButton::clicked, this, &FiltersPanel::HandleEdit);
    connect(m_removeButton, &QPushButton::clicked, this,
        &FiltersPanel::HandleRemove);
    connect(m_applyButton, &QPushButton::clicked, this, &FiltersPanel::HandleApply);
    connect(m_clearButton, &QPushButton::clicked, this, &FiltersPanel::HandleClear);
    connect(m_saveAsButton, &QPushButton::clicked, this,
        &FiltersPanel::HandleSaveAs);
    connect(m_loadButton, &QPushButton::clicked, this, &FiltersPanel::HandleLoad);
    connect(m_table, &QTableWidget::itemChanged, this,
        &FiltersPanel::HandleItemChanged);
    connect(m_table, &QTableWidget::itemSelectionChanged, this,
        &FiltersPanel::HandleSelectionChanged);
    connect(m_table, &QTableWidget::itemDoubleClicked, this,
        [this](QTableWidgetItem*) { HandleEdit(); });
}

void FiltersPanel::RefreshFilters()
{
    QSignalBlocker blocker(m_table);
    const auto& filters = filters::FilterManager::getInstance().getFilters();
    m_table->setRowCount(0);

    int row = 0;
    for (const auto& filter : filters)
    {
        if (!filter)
            continue;

        m_table->insertRow(row);

        auto* enabledItem = new QTableWidgetItem();
        enabledItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable |
            Qt::ItemIsSelectable);
        enabledItem->setCheckState(filter->isEnabled ? Qt::Checked : Qt::Unchecked);
        enabledItem->setData(Qt::UserRole,
            QString::fromStdString(filter->name));
        m_table->setItem(row, kEnabledColumn, enabledItem);

        auto* nameItem = new QTableWidgetItem(
            QString::fromStdString(filter->name));
        m_table->setItem(row, kNameColumn, nameItem);

        auto* targetItem = new QTableWidgetItem(FilterTargetText(*filter));
        m_table->setItem(row, kTargetColumn, targetItem);

        auto* patternItem = new QTableWidgetItem();
        if (filter->conditions.size() > 1)
        {
            patternItem->setText(tr("%1 conditions").arg(filter->conditions.size()));
            patternItem->setToolTip(tr("Complex filter with multiple AND conditions"));
        }
        else if (!filter->conditions.empty())
        {
            patternItem->setText(QString::fromStdString(filter->conditions[0].pattern));
        }
        else
        {
            patternItem->setText(QString::fromStdString(filter->pattern));
        }
        m_table->setItem(row, kPatternColumn, patternItem);

        if (!filter->isEnabled)
        {
            nameItem->setForeground(QColor(150, 150, 150));
            targetItem->setForeground(QColor(150, 150, 150));
            patternItem->setForeground(QColor(150, 150, 150));
        }

        ++row;
    }

    ToggleControls();
}

QString FiltersPanel::FilterTargetText(const filters::Filter& filter) const
{
    if (filter.conditions.size() > 1)
    {
        return tr("Multiple conditions (%1)").arg(filter.conditions.size());
    }
    else if (!filter.conditions.empty())
    {
        const auto& condition = filter.conditions[0];
        if (condition.isParameterFilter)
        {
            return tr("Parameter: %1").arg(
                QString::fromStdString(condition.parameterKey));
        }
        return tr("Column: %1").arg(QString::fromStdString(condition.columnName));
    }
    else if (filter.isParameterFilter)
    {
        return tr("Parameter: %1").arg(
            QString::fromStdString(filter.parameterKey));
    }
    return tr("Column: %1").arg(QString::fromStdString(filter.columnName));
}

void FiltersPanel::HandleAdd()
{
    FilterEditorDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    auto filter = dialog.GetFilter();
    if (!filter)
        return;

    filters::FilterManager::getInstance().addFilter(filter);
    HandleFilterResult(filters::FilterManager::getInstance().saveFilters(),
        tr("Saving filters after Add"));
    RefreshFilters();
    emit RequestApplyFilters();
}

QString FiltersPanel::SelectedFilterName() const
{
    const int row = m_table->currentRow();
    if (row < 0)
        return {};
    const auto* item = m_table->item(row, kNameColumn);
    return item ? item->text() : QString {};
}

void FiltersPanel::HandleEdit()
{
    const QString filterName = SelectedFilterName();
    if (filterName.isEmpty())
        return;

    auto filter = filters::FilterManager::getInstance().getFilterByName(
        filterName.toStdString());
    if (!filter)
        return;

    FilterEditorDialog dialog(this, std::make_shared<filters::Filter>(*filter));
    if (dialog.exec() != QDialog::Accepted)
        return;

    auto updatedFilter = dialog.GetFilter();
    if (!updatedFilter)
        return;

    filters::FilterManager::getInstance().updateFilter(updatedFilter);
    HandleFilterResult(filters::FilterManager::getInstance().saveFilters(),
        tr("Saving filters after Edit"));
    RefreshFilters();
    emit RequestApplyFilters();
}

void FiltersPanel::HandleRemove()
{
    const QString filterName = SelectedFilterName();
    if (filterName.isEmpty())
        return;

    if (QMessageBox::question(this, tr("Confirm"),
            tr("Remove filter '%1'?").arg(filterName)) != QMessageBox::Yes)
    {
        return;
    }

    filters::FilterManager::getInstance().removeFilter(
        filterName.toStdString());
    HandleFilterResult(filters::FilterManager::getInstance().saveFilters(),
        tr("Saving filters after Remove"));
    RefreshFilters();
    emit RequestApplyFilters();
}

void FiltersPanel::HandleApply()
{
    emit RequestApplyFilters();
}

void FiltersPanel::HandleClear()
{
    if (QMessageBox::question(this, tr("Confirm"),
            tr("Disable all filters?")) != QMessageBox::Yes)
    {
        return;
    }

    filters::FilterManager::getInstance().enableAllFilters(false);
    HandleFilterResult(filters::FilterManager::getInstance().saveFilters(),
        tr("Saving filters after Clear"));
    RefreshFilters();
    emit RequestApplyFilters();
}

void FiltersPanel::HandleSaveAs()
{

    QFileDialog dialog(this, tr("Save Filters"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    dialog.setNameFilter(tr("Filters (*.json)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.selectFile("filters.json");
    
    if (dialog.exec() != QDialog::Accepted)
        return;
    
    const QString path = dialog.selectedFiles().value(0);

    if (path.isEmpty())
        return;

    const auto result = filters::FilterManager::getInstance().saveFiltersToPath(
        path.toStdString());
    if (result.isErr())
    {
        HandleFilterResult(result, tr("Saving filters to custom path"), true);
        return;
    }

    QMessageBox::information(this, tr("Filters"),
        tr("Filters saved to %1").arg(path));
}

void FiltersPanel::HandleLoad()
{

    QFileDialog dialog(this, tr("Open Log File"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    dialog.setNameFilter(tr("Filter files (*.json);;All files (*.*)"));
    if (dialog.exec() != QDialog::Accepted) {
        util::Logger::Debug("[MainWindow] OnOpenFileRequested: dialog cancelled");
        return;
    }

    const QString path = dialog.selectedFiles().value(0);

    if (path.isEmpty())
        return;

    if (QMessageBox::question(this, tr("Confirm"),
            tr("Loading will replace existing filters. Continue?")) !=
        QMessageBox::Yes)
    {
        return;
    }

    const auto result = filters::FilterManager::getInstance().loadFiltersFromPath(
        path.toStdString());
    if (result.isErr())
    {
        HandleFilterResult(result, tr("Loading filters from file"), true);
        return;
    }

    RefreshFilters();
    emit RequestApplyFilters();
}

void FiltersPanel::HandleItemChanged(QTableWidgetItem* item)
{
    if (!item || item->column() != kEnabledColumn)
        return;

    const QString filterName = item->data(Qt::UserRole).toString();
    if (filterName.isEmpty())
        return;

    const bool enabled = item->checkState() == Qt::Checked;
    filters::FilterManager::getInstance().enableFilter(
        filterName.toStdString(), enabled);
    HandleFilterResult(filters::FilterManager::getInstance().saveFilters(),
        tr("Saving filters after Toggle"));
    RefreshFilters();
    emit RequestApplyFilters();
}

void FiltersPanel::HandleSelectionChanged()
{
    ToggleControls();
}

void FiltersPanel::ToggleControls()
{
    const bool hasSelection = m_table->currentRow() >= 0;
    m_editButton->setEnabled(hasSelection);
    m_removeButton->setEnabled(hasSelection);
}

void FiltersPanel::HandleFilterResult(const filters::FilterResult& result,
    const QString& context, bool showDialog)
{
    if (!result.isErr())
        return;

    const auto& err = result.error();
    util::Logger::Error("{}: {}", context.toStdString(), err.what());
    if (showDialog)
    {
        QMessageBox::critical(this, tr("Filters"),
            context + "\n" + QString::fromStdString(err.what()));
    }
}

} // namespace ui::qt
