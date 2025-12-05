#include "ui/qt/StructuredConfigDialog.hpp"

#include "config/Config.hpp"
#include "util/Logger.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QCheckBox>
#include <QSpinBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QColorDialog>
#include <QFrame>

#include <QGroupBox>

#include <algorithm>

namespace ui::qt
{

StructuredConfigDialog::StructuredConfigDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Configuration"));
    resize(700, 600);

    BuildUi();
    LoadConfigToUi();
}

void StructuredConfigDialog::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    mainLayout->addWidget(m_tabs, 1);

    InitGeneralTab();
    InitColumnsTab();
    InitColorsTab();

    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this,
        &StructuredConfigDialog::OnSaveClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this,
        &StructuredConfigDialog::OnCancelClicked);

    mainLayout->addWidget(buttonBox);
}

void StructuredConfigDialog::InitGeneralTab()
{
    m_generalTab = new QWidget(this);
    auto* layout = new QVBoxLayout(m_generalTab);

    // Config path + Open button
    auto* pathLayout = new QHBoxLayout();
    auto* pathLabel = new QLabel(tr("Config file:"), m_generalTab);
    m_configPathLabel = new QLabel(m_generalTab);
    auto font = m_configPathLabel->font();
    font.setItalic(true);
    m_configPathLabel->setFont(font);

    auto* openButton = new QPushButton(tr("Open"), m_generalTab);
    connect(openButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnOpenConfigFileClicked);

    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_configPathLabel, 1);
    pathLayout->addWidget(openButton);

    layout->addLayout(pathLayout);

    auto* form = new QFormLayout();
    m_xmlRootEdit = new QLineEdit(m_generalTab);
    m_xmlEventEdit = new QLineEdit(m_generalTab);
    m_logLevelCombo = new QComboBox(m_generalTab);

    m_logLevelCombo->addItems(
        {"trace", "debug", "info", "warning", "error", "critical", "off"});

    connect(m_logLevelCombo, &QComboBox::currentIndexChanged, this,
        &StructuredConfigDialog::OnLogLevelChanged);

    form->addRow(tr("xmlRootElement"), m_xmlRootEdit);
    form->addRow(tr("xmlEventElement"), m_xmlEventEdit);
    form->addRow(tr("logLevel"), m_logLevelCombo);

    layout->addLayout(form);
    layout->addStretch();

    m_tabs->addTab(m_generalTab, tr("General"));
}

void StructuredConfigDialog::InitColumnsTab()
{
    m_columnsTab = new QWidget(this);
    auto* layout = new QVBoxLayout(m_columnsTab);

    m_columnsTable = new QTableWidget(m_columnsTab);
    m_columnsTable->setColumnCount(3);
    QStringList headers;
    headers << tr("Visible") << tr("Column Name") << tr("Width");
    m_columnsTable->setHorizontalHeaderLabels(headers);
    m_columnsTable->horizontalHeader()->setStretchLastSection(true);
    m_columnsTable->verticalHeader()->setVisible(false);
    m_columnsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_columnsTable->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(m_columnsTable, &QTableWidget::itemSelectionChanged, this,
        &StructuredConfigDialog::OnColumnSelectionChanged);

    layout->addWidget(m_columnsTable, 1);

    auto* moveLayout = new QHBoxLayout();
    m_moveUpButton = new QPushButton(tr("Move Up"), m_columnsTab);
    m_moveDownButton = new QPushButton(tr("Move Down"), m_columnsTab);
    connect(m_moveUpButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnMoveColumnUp);
    connect(m_moveDownButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnMoveColumnDown);
    moveLayout->addWidget(m_moveUpButton);
    moveLayout->addWidget(m_moveDownButton);
    moveLayout->addStretch();
    layout->addLayout(moveLayout);

    auto* detailGroup = new QGroupBox(tr("Column Details"), m_columnsTab);
    auto* detailLayout = new QFormLayout(detailGroup);
    m_columnNameEdit = new QLineEdit(detailGroup);
    m_columnVisibleCheck = new QCheckBox(detailGroup);
    m_columnWidthSpin = new QSpinBox(detailGroup);
    m_columnWidthSpin->setRange(20, 500);

    detailLayout->addRow(tr("Name"), m_columnNameEdit);
    detailLayout->addRow(tr("Visible"), m_columnVisibleCheck);
    detailLayout->addRow(tr("Width"), m_columnWidthSpin);

    m_applyColumnChangesButton = new QPushButton(tr("Apply Changes"), detailGroup);
    connect(m_applyColumnChangesButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnApplyColumnChanges);

    auto* detailOuterLayout = new QVBoxLayout();
    detailOuterLayout->addWidget(detailGroup);
    detailOuterLayout->addWidget(m_applyColumnChangesButton, 0, Qt::AlignRight);

    layout->addLayout(detailOuterLayout);

    auto* buttonRow = new QHBoxLayout();
    m_addColumnButton = new QPushButton(tr("Add Column"), m_columnsTab);
    m_removeColumnButton = new QPushButton(tr("Remove Selected"), m_columnsTab);
    connect(m_addColumnButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnAddColumn);
    connect(m_removeColumnButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnRemoveColumn);
    buttonRow->addWidget(m_addColumnButton);
    buttonRow->addWidget(m_removeColumnButton);
    buttonRow->addStretch();

    layout->addLayout(buttonRow);

    m_tabs->addTab(m_columnsTab, tr("Columns"));
}

void StructuredConfigDialog::InitColorsTab()
{
    m_colorsTab = new QWidget(this);
    auto* layout = new QVBoxLayout(m_colorsTab);

    auto* columnRow = new QHBoxLayout();
    columnRow->addWidget(new QLabel(tr("Column:"), m_colorsTab));
    m_colorColumnCombo = new QComboBox(m_colorsTab);
    columnRow->addWidget(m_colorColumnCombo, 1);
    layout->addLayout(columnRow);

    m_colorColumnCombo->addItem(QString::fromLatin1("type"));
    connect(m_colorColumnCombo, &QComboBox::currentIndexChanged, this,
        &StructuredConfigDialog::OnColorColumnChanged);

    m_colorMappingsTable = new QTableWidget(m_colorsTab);
    m_colorMappingsTable->setColumnCount(4);
    QStringList headers;
    headers << tr("Value") << tr("Background Color") << tr("Text Color")
            << tr("Preview");
    m_colorMappingsTable->setHorizontalHeaderLabels(headers);
    m_colorMappingsTable->horizontalHeader()->setStretchLastSection(true);
    m_colorMappingsTable->verticalHeader()->setVisible(false);
    m_colorMappingsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_colorMappingsTable->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(m_colorMappingsTable, &QTableWidget::itemSelectionChanged, this,
        &StructuredConfigDialog::OnColorMappingSelectionChanged);

    layout->addWidget(m_colorMappingsTable, 1);

    auto* editGroup = new QGroupBox(tr("Add/Edit Color"), m_colorsTab);
    auto* editLayout = new QFormLayout(editGroup);

    m_colorValueEdit = new QLineEdit(editGroup);
    editLayout->addRow(tr("Value"), m_colorValueEdit);

    // Background controls
    auto* bgRow = new QHBoxLayout();
    m_bgColorSwatch = new QFrame(editGroup);
    m_bgColorSwatch->setFrameShape(QFrame::Box);
    m_bgColorSwatch->setFixedSize(30, 20);
    m_bgColorButton = new QPushButton(tr("Choose..."), editGroup);
    m_defaultBgButton = new QPushButton(tr("Default"), editGroup);
    bgRow->addWidget(m_bgColorSwatch);
    bgRow->addWidget(m_bgColorButton);
    bgRow->addWidget(m_defaultBgButton);

    connect(m_bgColorButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnBgColorButton);
    connect(m_defaultBgButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnDefaultBgColor);

    editLayout->addRow(tr("Background"), bgRow);

    // Foreground controls
    auto* fgRow = new QHBoxLayout();
    m_fgColorSwatch = new QFrame(editGroup);
    m_fgColorSwatch->setFrameShape(QFrame::Box);
    m_fgColorSwatch->setFixedSize(30, 20);
    m_fgColorButton = new QPushButton(tr("Choose..."), editGroup);
    m_defaultFgButton = new QPushButton(tr("Default"), editGroup);
    fgRow->addWidget(m_fgColorSwatch);
    fgRow->addWidget(m_fgColorButton);
    fgRow->addWidget(m_defaultFgButton);

    connect(m_fgColorButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnFgColorButton);
    connect(m_defaultFgButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnDefaultFgColor);

    editLayout->addRow(tr("Text"), fgRow);

    // Preview
    m_previewPanel = new QFrame(editGroup);
    m_previewPanel->setFrameShape(QFrame::StyledPanel);
    auto* previewLayout = new QVBoxLayout(m_previewPanel);
    m_previewLabel = new QLabel(tr("Sample Text"), m_previewPanel);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_previewLabel);

    editLayout->addRow(tr("Preview"), m_previewPanel);

    layout->addWidget(editGroup);

    auto* buttonRow = new QHBoxLayout();
    m_addColorButton = new QPushButton(tr("Add"), m_colorsTab);
    m_updateColorButton = new QPushButton(tr("Update"), m_colorsTab);
    m_deleteColorButton = new QPushButton(tr("Delete"), m_colorsTab);
    connect(m_addColorButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnAddColorMapping);
    connect(m_updateColorButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnUpdateColorMapping);
    connect(m_deleteColorButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnDeleteColorMapping);

    buttonRow->addWidget(m_addColorButton);
    buttonRow->addWidget(m_updateColorButton);
    buttonRow->addWidget(m_deleteColorButton);
    buttonRow->addStretch();

    layout->addLayout(buttonRow);

    m_tabs->addTab(m_colorsTab, tr("Colors"));

    // Default colors
    m_bgColor = QColor(Qt::white);
    m_fgColor = QColor(Qt::black);
    UpdateColorSwatches();
    UpdateColorPreview();
}

void StructuredConfigDialog::LoadConfigToUi()
{
    auto& cfg = config::GetConfig();

    const auto& path = cfg.GetConfigFilePath();
    m_configPathLabel->setText(QString::fromStdString(path));

    m_xmlRootEdit->setText(QString::fromStdString(cfg.xmlRootElement));
    m_xmlEventEdit->setText(QString::fromStdString(cfg.xmlEventElement));

    const QString logLevel = QString::fromStdString(cfg.logLevel);
    const int idx = m_logLevelCombo->findText(logLevel);
    if (idx >= 0)
        m_logLevelCombo->setCurrentIndex(idx);
    else
        m_logLevelCombo->setCurrentText(QString::fromLatin1("info"));

    RefreshColumnsList();
    RefreshColorMappings();
}

void StructuredConfigDialog::OnSaveClicked()
{
    auto& cfg = config::GetConfig();

    cfg.xmlRootElement = m_xmlRootEdit->text().toStdString();
    cfg.xmlEventElement = m_xmlEventEdit->text().toStdString();
    cfg.logLevel = m_logLevelCombo->currentText().toStdString();

    try
    {
        cfg.SaveConfig();
        // Don't reload after save - it would discard any in-memory changes
        // that were made through the UI but haven't been persisted yet
        NotifyObservers();
        QMessageBox::information(this, tr("Config"),
            tr("Configuration saved successfully."));
        accept();
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error(
            "[StructuredConfigDialog] Failed to save config: {}",
            ex.what());
        QMessageBox::warning(this, tr("Config"),
            tr("Failed to save configuration: %1").arg(ex.what()));
    }
}

void StructuredConfigDialog::OnCancelClicked()
{
    reject();
}

void StructuredConfigDialog::OnOpenConfigFileClicked()
{
    const auto& path = config::GetConfig().GetConfigFilePath();
    if (path.empty())
        return;

    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QString::fromStdString(path)));
}

void StructuredConfigDialog::OnLogLevelChanged(int)
{
    auto& cfg = config::GetConfig();
    const std::string levelStr =
        m_logLevelCombo->currentText().toStdString();
    cfg.logLevel = levelStr;

    const auto level = util::Logger::fromStrLevel(levelStr);
    util::Logger::SetLevel(level);
    util::Logger::Info("Log level changed to: {}", levelStr);
}

void StructuredConfigDialog::RefreshColumnsList()
{
    auto& cfg = config::GetConfig();
    const auto& columns = cfg.GetColumns();

    m_columnsTable->setRowCount(0);
    m_selectedColumnRow = -1;

    for (int i = 0; i < static_cast<int>(columns.size()); ++i)
    {
        const auto& col = columns[static_cast<std::size_t>(i)];
        const int row = m_columnsTable->rowCount();
        m_columnsTable->insertRow(row);

        auto* visibleItem = new QTableWidgetItem(
            col.isVisible ? tr("Yes") : tr("No"));
        auto* nameItem = new QTableWidgetItem(
            QString::fromStdString(col.name));
        auto* widthItem =
            new QTableWidgetItem(QString::number(col.width));

        visibleItem->setData(Qt::UserRole, i);
        nameItem->setData(Qt::UserRole, i);
        widthItem->setData(Qt::UserRole, i);

        m_columnsTable->setItem(row, 0, visibleItem);
        m_columnsTable->setItem(row, 1, nameItem);
        m_columnsTable->setItem(row, 2, widthItem);
    }
}

void StructuredConfigDialog::OnColumnSelectionChanged()
{
    const auto selected = m_columnsTable->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        m_selectedColumnRow = -1;
        return;
    }

    m_selectedColumnRow = selected.first().row();
    LoadSelectedColumnToEditors();
}

void StructuredConfigDialog::LoadSelectedColumnToEditors()
{
    if (m_selectedColumnRow < 0)
        return;

    auto* nameItem = m_columnsTable->item(m_selectedColumnRow, 1);
    auto* visibleItem = m_columnsTable->item(m_selectedColumnRow, 0);
    auto* widthItem = m_columnsTable->item(m_selectedColumnRow, 2);
    if (!nameItem || !visibleItem || !widthItem)
        return;

    const int configIndex = nameItem->data(Qt::UserRole).toInt();
    const auto& cfgColumns = config::GetConfig().GetColumns();
    if (configIndex < 0 ||
        configIndex >= static_cast<int>(cfgColumns.size()))
        return;

    const auto& col = cfgColumns[static_cast<std::size_t>(configIndex)];

    m_columnNameEdit->setText(QString::fromStdString(col.name));
    m_columnVisibleCheck->setChecked(col.isVisible);
    m_columnWidthSpin->setValue(col.width);
}

void StructuredConfigDialog::OnAddColumn()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Add Column"),
        tr("Enter new column name:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    auto& columns = config::GetConfig().GetMutableColumns();
    const std::string nameStr = name.toStdString();

    const bool duplicate = std::any_of(columns.begin(), columns.end(),
        [&](const config::ColumnConfig& c) { return c.name == nameStr; });
    if (duplicate)
    {
        QMessageBox::warning(this, tr("Duplicate Column"),
            tr("A column with this name already exists."));
        return;
    }

    config::ColumnConfig col;
    col.name = nameStr;
    col.isVisible = true;
    col.width = 100;
    columns.push_back(col);

    RefreshColumnsList();
}

void StructuredConfigDialog::OnRemoveColumn()
{
    if (m_selectedColumnRow < 0)
    {
        QMessageBox::information(this, tr("No Selection"),
            tr("Please select a column to remove."));
        return;
    }

    auto& columns = config::GetConfig().GetMutableColumns();
    if (columns.size() <= 1)
    {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot remove the last column."));
        return;
    }

    auto* nameItem = m_columnsTable->item(m_selectedColumnRow, 1);
    if (!nameItem)
        return;
    const QString colName = nameItem->text();

    const auto answer = QMessageBox::question(this, tr("Confirm Removal"),
        tr("Are you sure you want to remove column '%1'?").arg(colName));
    if (answer != QMessageBox::Yes)
        return;

    const int configIndex = nameItem->data(Qt::UserRole).toInt();
    if (configIndex < 0 ||
        configIndex >= static_cast<int>(columns.size()))
        return;

    columns.erase(columns.begin() + configIndex);
    RefreshColumnsList();
}

void StructuredConfigDialog::OnApplyColumnChanges()
{
    if (m_selectedColumnRow < 0)
    {
        QMessageBox::information(this, tr("No Selection"),
            tr("Please select a column first."));
        return;
    }

    auto& columns = config::GetConfig().GetMutableColumns();

    auto* nameItem = m_columnsTable->item(m_selectedColumnRow, 1);
    if (!nameItem)
        return;

    const int configIndex = nameItem->data(Qt::UserRole).toInt();
    if (configIndex < 0 ||
        configIndex >= static_cast<int>(columns.size()))
        return;

    const QString newName = m_columnNameEdit->text().trimmed();
    if (newName.isEmpty())
    {
        QMessageBox::warning(this, tr("Error"),
            tr("Column name cannot be empty."));
        return;
    }

    const std::string newNameStr = newName.toStdString();

    for (int i = 0; i < static_cast<int>(columns.size()); ++i)
    {
        if (i == configIndex)
            continue;
        if (columns[static_cast<std::size_t>(i)].name == newNameStr)
        {
            QMessageBox::warning(this, tr("Duplicate Column"),
                tr("A column with this name already exists."));
            return;
        }
    }

    columns[static_cast<std::size_t>(configIndex)].name = newNameStr;
    columns[static_cast<std::size_t>(configIndex)].isVisible =
        m_columnVisibleCheck->isChecked();
    columns[static_cast<std::size_t>(configIndex)].width =
        m_columnWidthSpin->value();

    RefreshColumnsList();
}

void StructuredConfigDialog::SwapColumns(int sourceRow, int targetRow)
{
    if (sourceRow == targetRow)
        return;

    auto& columns = config::GetConfig().GetMutableColumns();
    const auto& cols = config::GetConfig().GetColumns();

    if (sourceRow < 0 || targetRow < 0 ||
        sourceRow >= static_cast<int>(cols.size()) ||
        targetRow >= static_cast<int>(cols.size()))
        return;

    if (sourceRow < targetRow)
    {
        std::rotate(columns.begin() + sourceRow,
            columns.begin() + sourceRow + 1,
            columns.begin() + targetRow + 1);
    }
    else
    {
        std::rotate(columns.begin() + targetRow,
            columns.begin() + sourceRow,
            columns.begin() + sourceRow + 1);
    }

    RefreshColumnsList();

    m_selectedColumnRow = targetRow;
    m_columnsTable->selectRow(targetRow);
}

void StructuredConfigDialog::OnMoveColumnUp()
{
    if (m_selectedColumnRow <= 0)
        return;

    const int newRow = m_selectedColumnRow - 1;
    SwapColumns(m_selectedColumnRow, newRow);
}

void StructuredConfigDialog::OnMoveColumnDown()
{
    if (m_selectedColumnRow < 0)
        return;

    const int rowCount = m_columnsTable->rowCount();
    if (m_selectedColumnRow >= rowCount - 1)
        return;

    const int newRow = m_selectedColumnRow + 1;
    SwapColumns(m_selectedColumnRow, newRow);
}

void StructuredConfigDialog::OnColorColumnChanged(int)
{
    RefreshColorMappings();
}

void StructuredConfigDialog::RefreshColorMappings()
{
    m_colorMappingsTable->setRowCount(0);

    const QString columnName = m_colorColumnCombo->currentText();
    if (columnName.isEmpty())
        return;

    const auto& columnColors = config::GetConfig().columnColors;
    auto it = columnColors.find(columnName.toStdString());
    if (it == columnColors.end())
        return;

    int row = 0;
    for (const auto& mapping : it->second)
    {
        const QString value = QString::fromStdString(mapping.first);
        const QString bg = QString::fromStdString(mapping.second.bg);
        const QString fg = QString::fromStdString(mapping.second.fg);

        m_colorMappingsTable->insertRow(row);

        auto* valueItem = new QTableWidgetItem(value);
        auto* bgItem = new QTableWidgetItem(bg);
        auto* fgItem = new QTableWidgetItem(fg);
        auto* previewItem = new QTableWidgetItem(tr("Preview"));

        const QColor bgColor = HexToColor(bg);
        const QColor fgColor = HexToColor(fg);
        previewItem->setBackground(bgColor);
        previewItem->setForeground(fgColor);

        m_colorMappingsTable->setItem(row, 0, valueItem);
        m_colorMappingsTable->setItem(row, 1, bgItem);
        m_colorMappingsTable->setItem(row, 2, fgItem);
        m_colorMappingsTable->setItem(row, 3, previewItem);

        ++row;
    }
}

void StructuredConfigDialog::OnColorMappingSelectionChanged()
{
    const auto selected = m_colorMappingsTable->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;

    const int row = selected.first().row();
    UpdateColorEditorsFromSelection(row);
}

void StructuredConfigDialog::UpdateColorEditorsFromSelection(int row)
{
    auto* valueItem = m_colorMappingsTable->item(row, 0);
    auto* bgItem = m_colorMappingsTable->item(row, 1);
    auto* fgItem = m_colorMappingsTable->item(row, 2);
    if (!valueItem || !bgItem || !fgItem)
        return;

    m_colorValueEdit->setText(valueItem->text());

    m_bgColor = HexToColor(bgItem->text());
    m_fgColor = HexToColor(fgItem->text());

    UpdateColorSwatches();
    UpdateColorPreview();
}

void StructuredConfigDialog::OnBgColorButton()
{
    const QColor chosen = QColorDialog::getColor(m_bgColor, this);
    if (!chosen.isValid())
        return;

    m_bgColor = chosen;
    UpdateColorSwatches();
    UpdateColorPreview();
}

void StructuredConfigDialog::OnFgColorButton()
{
    const QColor chosen = QColorDialog::getColor(m_fgColor, this);
    if (!chosen.isValid())
        return;

    m_fgColor = chosen;
    UpdateColorSwatches();
    UpdateColorPreview();
}

void StructuredConfigDialog::OnDefaultBgColor()
{
    m_bgColor = QColor(Qt::white);
    UpdateColorSwatches();
    UpdateColorPreview();
}

void StructuredConfigDialog::OnDefaultFgColor()
{
    m_fgColor = QColor(Qt::black);
    UpdateColorSwatches();
    UpdateColorPreview();
}

void StructuredConfigDialog::OnAddColorMapping()
{
    const QString columnName = m_colorColumnCombo->currentText();
    const QString value = m_colorValueEdit->text().trimmed();

    if (columnName.isEmpty())
    {
        QMessageBox::information(this, tr("No Column Selected"),
            tr("Please select a column first."));
        return;
    }

    if (value.isEmpty())
    {
        QMessageBox::information(this, tr("Empty Value"),
            tr("Please enter a value for the color mapping."));
        return;
    }

    auto& columnColors = config::GetConfig().columnColors;
    auto& mappings = columnColors[columnName.toStdString()];

    if (mappings.find(value.toStdString()) != mappings.end())
    {
        QMessageBox::information(this, tr("Duplicate Mapping"),
            tr("A color mapping for this value already exists. Use Update to modify it."));
        return;
    }

    config::ColumnColor col;
    col.bg = ColorToHex(m_bgColor).toStdString();
    col.fg = ColorToHex(m_fgColor).toStdString();
    mappings[value.toStdString()] = col;

    RefreshColorMappings();
}

void StructuredConfigDialog::OnUpdateColorMapping()
{
    const QString columnName = m_colorColumnCombo->currentText();
    const QString value = m_colorValueEdit->text().trimmed();

    if (columnName.isEmpty() || value.isEmpty())
    {
        QMessageBox::information(this, tr("Missing Information"),
            tr("Please select a column and enter a value."));
        return;
    }

    auto& columnColors = config::GetConfig().columnColors;
    auto& mappings = columnColors[columnName.toStdString()];

    config::ColumnColor col;
    col.bg = ColorToHex(m_bgColor).toStdString();
    col.fg = ColorToHex(m_fgColor).toStdString();
    mappings[value.toStdString()] = col;

    RefreshColorMappings();
}

void StructuredConfigDialog::OnDeleteColorMapping()
{
    const QString columnName = m_colorColumnCombo->currentText();
    if (columnName.isEmpty())
    {
        QMessageBox::information(this, tr("No Column Selected"),
            tr("Please select a column first."));
        return;
    }

    const auto selected = m_colorMappingsTable->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        QMessageBox::information(this, tr("No Selection"),
            tr("Please select a color mapping to delete."));
        return;
    }

    const int row = selected.first().row();
    auto* valueItem = m_colorMappingsTable->item(row, 0);
    if (!valueItem)
        return;

    const QString value = valueItem->text();

    const auto answer = QMessageBox::question(this, tr("Confirm Deletion"),
        tr("Are you sure you want to delete the color mapping for '%1'?").arg(value));
    if (answer != QMessageBox::Yes)
        return;

    auto& columnColors = config::GetConfig().columnColors;
    auto it = columnColors.find(columnName.toStdString());
    if (it == columnColors.end())
        return;

    it->second.erase(value.toStdString());
    if (it->second.empty())
        columnColors.erase(it);

    RefreshColorMappings();
}

void StructuredConfigDialog::UpdateColorSwatches()
{
    if (m_bgColorSwatch)
    {
        m_bgColorSwatch->setStyleSheet(
            QString::fromLatin1("background-color: %1;")
                .arg(ColorToHex(m_bgColor)));
    }
    if (m_fgColorSwatch)
    {
        m_fgColorSwatch->setStyleSheet(
            QString::fromLatin1("background-color: %1;")
                .arg(ColorToHex(m_fgColor)));
    }
}

void StructuredConfigDialog::UpdateColorPreview()
{
    if (!m_previewPanel || !m_previewLabel)
        return;

    m_previewPanel->setStyleSheet(
        QString::fromLatin1("background-color: %1;")
            .arg(ColorToHex(m_bgColor)));
    m_previewLabel->setStyleSheet(
        QString::fromLatin1("color: %1;")
            .arg(ColorToHex(m_fgColor)));

    const QString value = m_colorValueEdit->text().trimmed();
    m_previewLabel->setText(value.isEmpty() ? tr("Sample Text") : value);
}

QString StructuredConfigDialog::ColorToHex(const QColor& color) const
{
    return QString::fromLatin1("#%1%2%3")
        .arg(color.red(), 2, 16, QLatin1Char('0'))
        .arg(color.green(), 2, 16, QLatin1Char('0'))
        .arg(color.blue(), 2, 16, QLatin1Char('0'));
}

QColor StructuredConfigDialog::HexToColor(const QString& hex) const
{
    if (hex.isEmpty() || !hex.startsWith('#') || hex.length() != 7)
        return QColor(Qt::white);

    QColor c(hex);
    if (!c.isValid())
        return QColor(Qt::white);

    return c;
}

void StructuredConfigDialog::AddObserver(config::ConfigObserver* observer)
{
    if (!observer)
        return;

    if (std::find(m_observers.begin(), m_observers.end(), observer) ==
        m_observers.end())
    {
        m_observers.push_back(observer);
    }
}

void StructuredConfigDialog::RemoveObserver(config::ConfigObserver* observer)
{
    if (!observer)
        return;

    m_observers.erase(
        std::remove(m_observers.begin(), m_observers.end(), observer),
        m_observers.end());
}

void StructuredConfigDialog::NotifyObservers()
{
    for (auto* observer : m_observers)
    {
        if (observer)
            observer->OnConfigChanged();
    }
}

} // namespace ui::qt
