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
#include <QMessageBox>
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
    resize(500, 420);
    setMinimumSize(420, 360);

    if (m_filter)
    {
        m_nameEdit->setText(QString::fromStdString(m_filter->name));
        m_patternEdit->setText(QString::fromStdString(m_filter->pattern));
        m_caseSensitiveCheck->setChecked(m_filter->isCaseSensitive);
        m_invertedCheck->setChecked(m_filter->isInverted);

        if (m_filter->isParameterFilter)
        {
            m_filterTypeCombo->setCurrentIndex(kParameterFilterIndex);
            m_filterPages->setCurrentIndex(kParameterFilterIndex);
            m_parameterKeyEdit->setText(
                QString::fromStdString(m_filter->parameterKey));
            m_depthSpin->setValue(m_filter->parameterDepth);
        }
        else
        {
            m_filterTypeCombo->setCurrentIndex(kColumnFilterIndex);
            m_filterPages->setCurrentIndex(kColumnFilterIndex);
            const QString columnName =
                QString::fromStdString(m_filter->columnName);
            const int idx = m_columnCombo->findText(columnName);
            if (idx >= 0)
                m_columnCombo->setCurrentIndex(idx);
        }
    }

    connect(m_filterTypeCombo, &QComboBox::currentIndexChanged,
        this, &FilterEditorDialog::OnFilterTypeChanged);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this,
        &FilterEditorDialog::HandleAccept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this,
        &FilterEditorDialog::reject);
}

void FilterEditorDialog::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel(tr("Filter Type:"), this));
    m_filterTypeCombo = new QComboBox(this);
    m_filterTypeCombo->addItem(tr("Column Filter"));
    m_filterTypeCombo->addItem(tr("Parameter Filter"));
    typeLayout->addWidget(m_filterTypeCombo, 1);
    mainLayout->addLayout(typeLayout);

    m_filterPages = new QStackedWidget(this);
    m_filterPages->addWidget(BuildColumnPage());
    m_filterPages->addWidget(BuildParameterPage());
    mainLayout->addWidget(m_filterPages, 1);

    auto* formLayout = new QFormLayout();
    m_nameEdit = new QLineEdit(this);
    formLayout->addRow(tr("Name:"), m_nameEdit);

    m_patternEdit = new QLineEdit(this);
    formLayout->addRow(tr("Pattern (regex):"), m_patternEdit);

    auto* optionsLayout = new QHBoxLayout();
    m_caseSensitiveCheck = new QCheckBox(tr("Case sensitive"), this);
    m_invertedCheck = new QCheckBox(tr("Inverted match"), this);
    optionsLayout->addWidget(m_caseSensitiveCheck);
    optionsLayout->addWidget(m_invertedCheck);
    formLayout->addRow(tr("Options:"), optionsLayout);
    mainLayout->addLayout(formLayout);

    m_buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(m_buttonBox);

    m_filterTypeCombo->setCurrentIndex(kColumnFilterIndex);
    m_filterPages->setCurrentIndex(kColumnFilterIndex);
}

QWidget* FilterEditorDialog::BuildColumnPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QFormLayout(page);
    m_columnCombo = new QComboBox(page);
    PopulateColumns();
    layout->addRow(tr("Column:"), m_columnCombo);
    layout->addRow(new QLabel(
        tr("Column filters match against a specific column value."), page));
    return page;
}

QWidget* FilterEditorDialog::BuildParameterPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QFormLayout(page);
    m_parameterKeyEdit = new QLineEdit(page);
    layout->addRow(tr("Parameter Key:"), m_parameterKeyEdit);
    m_depthSpin = new QSpinBox(page);
    m_depthSpin->setRange(0, 5);
    layout->addRow(tr("Search Depth:"), m_depthSpin);
    layout->addRow(new QLabel(tr("Parameter filters search nested key/value pairs."),
        page));
    return page;
}

void FilterEditorDialog::PopulateColumns()
{
    if (!m_columnCombo)
        return;

    m_columnCombo->clear();
    const auto& columns = config::GetConfig().columns;
    for (const auto& column : columns)
    {
        m_columnCombo->addItem(QString::fromStdString(column.name));
    }
    m_columnCombo->addItem("*");
}

void FilterEditorDialog::OnFilterTypeChanged(int index)
{
    if (!m_filterPages)
        return;
    m_filterPages->setCurrentIndex(index);
}

void FilterEditorDialog::HandleAccept()
{
    if (ValidateAndPersist())
        accept();
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

    const QString pattern = m_patternEdit->text().trimmed();
    if (pattern.isEmpty())
    {
        QMessageBox::warning(this, tr("Validation"),
            tr("Pattern cannot be empty."));
        m_patternEdit->setFocus();
        return false;
    }

    const bool isParameterFilter =
        m_filterTypeCombo->currentIndex() == kParameterFilterIndex;

    QString columnName;
    QString parameterKey;
    int depth = 0;

    if (isParameterFilter)
    {
        parameterKey = m_parameterKeyEdit->text().trimmed();
        if (parameterKey.isEmpty())
        {
            QMessageBox::warning(this, tr("Validation"),
                tr("Parameter key cannot be empty."));
            m_parameterKeyEdit->setFocus();
            return false;
        }
        depth = m_depthSpin->value();
    }
    else
    {
        if (m_columnCombo->currentIndex() < 0)
        {
            QMessageBox::warning(this, tr("Validation"),
                tr("Please select a column."));
            m_columnCombo->setFocus();
            return false;
        }
        columnName = m_columnCombo->currentText();
    }

    try
    {
        std::regex testRegex(pattern.toStdString(),
            m_caseSensitiveCheck->isChecked()
                ? std::regex::ECMAScript
                : std::regex::ECMAScript | std::regex::icase);
        (void)testRegex;
    }
    catch (const std::regex_error& ex)
    {
        QMessageBox::warning(this, tr("Validation"),
            tr("Invalid regular expression: %1").arg(ex.what()));
        m_patternEdit->setFocus();
        return false;
    }

    const auto nameStd = name.toStdString();
    const auto patternStd = pattern.toStdString();

    if (!m_filter)
    {
        m_filter = std::make_shared<filters::Filter>(nameStd,
            columnName.toStdString(), patternStd,
            m_caseSensitiveCheck->isChecked(),
            m_invertedCheck->isChecked(), isParameterFilter,
            parameterKey.toStdString(), depth);
        return true;
    }

    if (m_filter->name != nameStd)
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

    m_filter->name = nameStd;
    m_filter->pattern = patternStd;
    m_filter->isCaseSensitive = m_caseSensitiveCheck->isChecked();
    m_filter->isInverted = m_invertedCheck->isChecked();
    m_filter->isParameterFilter = isParameterFilter;
    if (isParameterFilter)
    {
        m_filter->parameterKey = parameterKey.toStdString();
        m_filter->parameterDepth = depth;
        m_filter->columnName = "*";
    }
    else
    {
        m_filter->columnName = columnName.toStdString();
        m_filter->parameterKey.clear();
        m_filter->parameterDepth = 0;
    }
    m_filter->compile();
    return true;
}

} // namespace ui::qt
