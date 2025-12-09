#include "ui/qt/FilterEditorDialog.hpp"

#include "config/Config.hpp"
#include "filters/FilterManager.hpp"
#include "util/Logger.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <regex>

namespace ui::qt
{

namespace
{
constexpr int kColumnFilterIndex = 0;
constexpr int kParameterFilterIndex = 1;
}

FilterEditorDialog::FilterEditorDialog(
    QWidget* parent, filters::FilterPtr filter)
    : QDialog(parent)
    , m_filter(std::move(filter))
{
    setWindowTitle(m_filter ? tr("Edit Filter") : tr("New Filter"));
    setModal(true);
    BuildUi();
    resize(600, 500);
    setMinimumSize(500, 400);

    if (m_filter)
    {
        m_nameEdit->setText(QString::fromStdString(m_filter->name));
        m_invertedCheck->setChecked(m_filter->isInverted);

        // Load existing conditions or create default single condition
        if (!m_filter->conditions.empty())
        {
            m_conditions = m_filter->conditions;
        }
        else
        {
            // Backward compatibility: create condition from legacy fields
            filters::FilterCondition condition(
                m_filter->columnName, m_filter->pattern,
                m_filter->isParameterFilter, m_filter->parameterKey, m_filter->parameterDepth,
                m_filter->isCaseSensitive);
            m_conditions.push_back(std::move(condition));
        }
    }
    else
    {
        // New filter: start with one empty condition
        m_conditions.emplace_back();
    }

    UpdateConditionList();

    connect(m_filterTypeCombo, &QComboBox::currentIndexChanged,
        this, &FilterEditorDialog::OnFilterTypeChanged);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this,
        &FilterEditorDialog::HandleAccept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this,
        &FilterEditorDialog::reject);
    connect(m_addConditionBtn, &QPushButton::clicked, this,
        &FilterEditorDialog::OnAddCondition);
    connect(m_removeConditionBtn, &QPushButton::clicked, this,
        &FilterEditorDialog::OnRemoveCondition);
    connect(m_conditionsList, &QListWidget::currentRowChanged, this,
        &FilterEditorDialog::OnConditionSelectionChanged);
    connect(m_conditionsList, &QListWidget::itemDoubleClicked, this,
        &FilterEditorDialog::OnConditionItemDoubleClicked);
}

void FilterEditorDialog::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Filter name
    auto* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(tr("Filter Name:"), this));
    m_nameEdit = new QLineEdit(this);
    nameLayout->addWidget(m_nameEdit, 1);
    mainLayout->addLayout(nameLayout);

    // Conditions section
    auto* conditionsGroup = new QVBoxLayout();
    conditionsGroup->addWidget(new QLabel(tr("Conditions (AND logic):"), this));

    // Conditions list and buttons
    auto* conditionsLayout = new QHBoxLayout();
    m_conditionsList = new QListWidget(this);
    m_conditionsList->setMaximumHeight(120);
    conditionsLayout->addWidget(m_conditionsList, 1);

    auto* conditionButtons = new QVBoxLayout();
    m_addConditionBtn = new QPushButton(tr("Add condition"), this);
    m_removeConditionBtn = new QPushButton(tr("Remove"), this);
    m_removeConditionBtn->setEnabled(false);
    conditionButtons->addWidget(m_addConditionBtn);
    conditionButtons->addWidget(m_removeConditionBtn);
    conditionButtons->addStretch();
    conditionsLayout->addLayout(conditionButtons);

    conditionsGroup->addLayout(conditionsLayout);
    mainLayout->addLayout(conditionsGroup);

    // Condition editor
    m_conditionEditorWidget = new QWidget(this);
    auto* editorLayout = new QVBoxLayout(m_conditionEditorWidget);

    // Condition type selector
    auto* typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel(tr("Condition Type:"), this));
    m_conditionTypeCombo = new QComboBox(this);
    m_conditionTypeCombo->addItem(tr("Column Filter"));
    m_conditionTypeCombo->addItem(tr("Parameter Filter"));
    typeLayout->addWidget(m_conditionTypeCombo, 1);
    editorLayout->addLayout(typeLayout);

    // Condition pages
    auto* conditionPages = new QStackedWidget(this);
    conditionPages->addWidget(BuildColumnPage());
    conditionPages->addWidget(BuildParameterPage());
    editorLayout->addWidget(conditionPages);

    // Pattern and options
    auto* patternLayout = new QFormLayout();
    m_conditionPatternEdit = new QLineEdit(this);
    patternLayout->addRow(tr("Pattern (regex):"), m_conditionPatternEdit);

    auto* optionsLayout = new QHBoxLayout();
    m_conditionCaseSensitiveCheck = new QCheckBox(tr("Case sensitive"), this);
    optionsLayout->addWidget(m_conditionCaseSensitiveCheck);
    optionsLayout->addStretch();
    patternLayout->addRow(tr("Options:"), optionsLayout);
    editorLayout->addLayout(patternLayout);

    mainLayout->addWidget(m_conditionEditorWidget);

    // Filter-level options
    auto* filterOptionsLayout = new QHBoxLayout();
    m_invertedCheck = new QCheckBox(tr("Invert entire filter (NOT logic)"), this);
    filterOptionsLayout->addWidget(m_invertedCheck);
    filterOptionsLayout->addStretch();
    mainLayout->addLayout(filterOptionsLayout);

    // Buttons
    m_buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    // Connect condition type changes
    connect(m_conditionTypeCombo, &QComboBox::currentIndexChanged,
        conditionPages, &QStackedWidget::setCurrentIndex);
}

QWidget* FilterEditorDialog::BuildColumnPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QFormLayout(page);
    m_conditionColumnCombo = new QComboBox(page);
    PopulateColumns();
    layout->addRow(tr("Column:"), m_conditionColumnCombo);
    layout->addRow(new QLabel(
        tr("Column filters match against a specific column value."), page));
    return page;
}

QWidget* FilterEditorDialog::BuildParameterPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QFormLayout(page);
    m_conditionParameterKeyEdit = new QLineEdit(page);
    layout->addRow(tr("Parameter Key:"), m_conditionParameterKeyEdit);
    m_conditionDepthSpin = new QSpinBox(page);
    m_conditionDepthSpin->setRange(0, 5);
    layout->addRow(tr("Search Depth:"), m_conditionDepthSpin);
    layout->addRow(new QLabel(tr("Parameter filters search nested key/value pairs."),
        page));
    return page;
}

void FilterEditorDialog::PopulateColumns()
{
    if (!m_conditionColumnCombo)
        return;

    m_conditionColumnCombo->clear();
    const auto& columns = config::GetConfig().columns;
    for (const auto& column : columns)
    {
        m_conditionColumnCombo->addItem(QString::fromStdString(column.name));
    }
    m_conditionColumnCombo->addItem("*");
}

void FilterEditorDialog::OnFilterTypeChanged(int index)
{
    // This method is no longer used in the new design
    Q_UNUSED(index)
}

void FilterEditorDialog::HandleAccept()
{
    if (ValidateAndPersist())
        accept();
}

void FilterEditorDialog::OnAddCondition()
{
    // Create new condition from current editor values
    filters::FilterCondition newCondition = CreateConditionFromEditor();
    m_conditions.push_back(newCondition);
    UpdateConditionList();

    // Select the new condition
    const int newIndex = static_cast<int>(m_conditions.size()) - 1;
    m_conditionsList->setCurrentRow(newIndex);
    LoadConditionIntoEditor(newIndex);
}

void FilterEditorDialog::OnRemoveCondition()
{
    const int currentRow = m_conditionsList->currentRow();
    if (currentRow < 0 || m_conditions.size() <= 1)
        return;

    // Remove the condition
    m_conditions.erase(m_conditions.begin() + currentRow);
    UpdateConditionList();

    // Select appropriate replacement
    const int newRow = std::min(currentRow, static_cast<int>(m_conditions.size()) - 1);
    if (newRow >= 0)
    {
        m_conditionsList->setCurrentRow(newRow);
        LoadConditionIntoEditor(newRow);
    }
    else
    {
        ClearConditionEditor();
        m_currentEditingCondition = -1;
    }
}

void FilterEditorDialog::OnConditionSelectionChanged()
{
    const int currentRow = m_conditionsList->currentRow();
    if (currentRow < 0)
    {
        ClearConditionEditor();
        m_currentEditingCondition = -1;
        m_removeConditionBtn->setEnabled(false);
        return;
    }

    // Save previous condition
    if (m_currentEditingCondition >= 0 && m_currentEditingCondition != currentRow)
    {
        SaveConditionFromEditor(m_currentEditingCondition);
    }

    // Load new condition
    LoadConditionIntoEditor(currentRow);
    m_removeConditionBtn->setEnabled(m_conditions.size() > 1);
}

void FilterEditorDialog::OnConditionItemDoubleClicked(QListWidgetItem* item)
{
    Q_UNUSED(item)
    // Could implement inline editing here if desired
}

void FilterEditorDialog::UpdateConditionList()
{
    m_conditionsList->clear();
    for (size_t i = 0; i < m_conditions.size(); ++i)
    {
        const auto& condition = m_conditions[i];
        QString description;

        if (condition.isParameterFilter)
        {
            description = tr("Parameter '%1' matches '%2'")
                .arg(QString::fromStdString(condition.parameterKey))
                .arg(QString::fromStdString(condition.pattern));
        }
        else
        {
            description = tr("Column '%1' matches '%2'")
                .arg(QString::fromStdString(condition.columnName))
                .arg(QString::fromStdString(condition.pattern));
        }

        auto* item = new QListWidgetItem(description, m_conditionsList);
        item->setData(Qt::UserRole, static_cast<int>(i));
    }
}

void FilterEditorDialog::LoadConditionIntoEditor(int conditionIndex)
{
    if (conditionIndex < 0 || conditionIndex >= static_cast<int>(m_conditions.size()))
        return;

    const auto& condition = m_conditions[conditionIndex];

    if (condition.isParameterFilter)
    {
        m_conditionTypeCombo->setCurrentIndex(kParameterFilterIndex);
        m_conditionParameterKeyEdit->setText(QString::fromStdString(condition.parameterKey));
        m_conditionDepthSpin->setValue(condition.parameterDepth);
    }
    else
    {
        m_conditionTypeCombo->setCurrentIndex(kColumnFilterIndex);
        const QString columnName = QString::fromStdString(condition.columnName);
        const int idx = m_conditionColumnCombo->findText(columnName);
        if (idx >= 0)
            m_conditionColumnCombo->setCurrentIndex(idx);
    }

    m_conditionPatternEdit->setText(QString::fromStdString(condition.pattern));
    m_conditionCaseSensitiveCheck->setChecked(condition.isCaseSensitive);

    m_currentEditingCondition = conditionIndex;
}

void FilterEditorDialog::SaveConditionFromEditor(int conditionIndex)
{
    if (conditionIndex < 0 || conditionIndex >= static_cast<int>(m_conditions.size()))
        return;

    auto& condition = m_conditions[conditionIndex];
    condition = CreateConditionFromEditor();
}

filters::FilterCondition FilterEditorDialog::CreateConditionFromEditor()
{
    filters::FilterCondition condition;

    const bool isParameterFilter = m_conditionTypeCombo->currentIndex() == kParameterFilterIndex;

    if (isParameterFilter)
    {
        condition.isParameterFilter = true;
        condition.parameterKey = m_conditionParameterKeyEdit->text().toStdString();
        condition.parameterDepth = m_conditionDepthSpin->value();
        condition.columnName = "*";
    }
    else
    {
        condition.isParameterFilter = false;
        condition.columnName = m_conditionColumnCombo->currentText().toStdString();
        condition.parameterKey.clear();
        condition.parameterDepth = 0;
    }

    condition.pattern = m_conditionPatternEdit->text().toStdString();

    // Create strategy based on case sensitivity
    condition.strategy = std::make_unique<filters::RegexFilterStrategy>();
    condition.isCaseSensitive = m_conditionCaseSensitiveCheck->isChecked();

    return condition;
}

void FilterEditorDialog::ClearConditionEditor()
{
    m_conditionTypeCombo->setCurrentIndex(kColumnFilterIndex);
    m_conditionColumnCombo->setCurrentIndex(0);
    m_conditionParameterKeyEdit->clear();
    m_conditionDepthSpin->setValue(0);
    m_conditionPatternEdit->clear();
    m_conditionCaseSensitiveCheck->setChecked(false);
}

bool FilterEditorDialog::ValidateAndPersist()
{
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("Validation"),
            tr("Filter name cannot be empty."));
        m_nameEdit->setFocus();
        return false;
    }

    // Save any pending condition edits
    if (m_currentEditingCondition >= 0)
    {
        SaveConditionFromEditor(m_currentEditingCondition);
    }

    // Validate all conditions
    for (size_t i = 0; i < m_conditions.size(); ++i)
    {
        const auto& condition = m_conditions[i];

        if (condition.pattern.empty())
        {
            QMessageBox::warning(this, tr("Validation"),
                tr("Condition %1: Pattern cannot be empty.").arg(i + 1));
            m_conditionsList->setCurrentRow(static_cast<int>(i));
            LoadConditionIntoEditor(static_cast<int>(i));
            m_conditionPatternEdit->setFocus();
            return false;
        }

        if (condition.isParameterFilter)
        {
            if (condition.parameterKey.empty())
            {
                QMessageBox::warning(this, tr("Validation"),
                    tr("Condition %1: Parameter key cannot be empty.").arg(i + 1));
                m_conditionsList->setCurrentRow(static_cast<int>(i));
                LoadConditionIntoEditor(static_cast<int>(i));
                m_conditionParameterKeyEdit->setFocus();
                return false;
            }
        }
        else
        {
            if (condition.columnName.empty())
            {
                QMessageBox::warning(this, tr("Validation"),
                    tr("Condition %1: Column cannot be empty.").arg(i + 1));
                m_conditionsList->setCurrentRow(static_cast<int>(i));
                LoadConditionIntoEditor(static_cast<int>(i));
                m_conditionColumnCombo->setFocus();
                return false;
            }
        }

        // Validate regex pattern
        try
        {
            std::regex testRegex(condition.pattern,
                condition.strategy ? std::regex::ECMAScript : std::regex::ECMAScript | std::regex::icase);
            (void)testRegex;
        }
        catch (const std::regex_error& ex)
        {
            QMessageBox::warning(this, tr("Validation"),
                tr("Condition %1: Invalid regular expression: %2").arg(i + 1).arg(ex.what()));
            m_conditionsList->setCurrentRow(static_cast<int>(i));
            LoadConditionIntoEditor(static_cast<int>(i));
            m_conditionPatternEdit->setFocus();
            return false;
        }
    }

    const auto nameStd = name.toStdString();

    if (!m_filter)
    {
        m_filter = std::make_shared<filters::Filter>(nameStd);
    }
    else if (m_filter->name != nameStd)
    {
        auto existing = filters::FilterManager::getInstance().getFilterByName(nameStd);
        if (existing)
        {
            QMessageBox::warning(this, tr("Validation"),
                tr("A filter with this name already exists."));
            m_nameEdit->setFocus();
            return false;
        }
    }

    // Update filter properties
    m_filter->name = nameStd;
    m_filter->isInverted = m_invertedCheck->isChecked();
    m_filter->isEnabled = true; // Default to enabled

    // Set conditions
    m_filter->conditions = m_conditions;

    // Clear legacy fields (for backward compatibility, they will be derived from first condition)
    if (!m_conditions.empty())
    {
        const auto& firstCondition = m_conditions[0];
        m_filter->columnName = firstCondition.columnName;
        m_filter->pattern = firstCondition.pattern;
        m_filter->isParameterFilter = firstCondition.isParameterFilter;
        m_filter->parameterKey = firstCondition.parameterKey;
        m_filter->parameterDepth = firstCondition.parameterDepth;
        m_filter->isCaseSensitive = firstCondition.isCaseSensitive;
    }

    m_filter->compile();
    return true;
}

} // namespace ui::qt
