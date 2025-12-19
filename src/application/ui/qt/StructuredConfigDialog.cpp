#include "StructuredConfigDialog.hpp"

#include "Config.hpp"
#include "FieldConversionPluginRegistry.hpp"
#include "Logger.hpp"
#include "PluginManager.hpp"

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
#include <QFileDialog>

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
    InitDictionaryTab();
    InitColorsTab();
    InitItemHighlightsTab();
    InitPluginsTab();

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

    // Config path + buttons
    auto* pathLayout = new QHBoxLayout();
    auto* pathLabel = new QLabel(tr("Config file:"), m_generalTab);
    m_configPathLabel = new QLabel(m_generalTab);
    auto font = m_configPathLabel->font();
    font.setItalic(true);
    m_configPathLabel->setFont(font);

    auto* openButton = new QPushButton(tr("Open"), m_generalTab);
    connect(openButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnOpenConfigFileClicked);
    
    auto* loadButton = new QPushButton(tr("Load..."), m_generalTab);
    loadButton->setToolTip(tr("Load configuration from another file"));
    connect(loadButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnLoadConfigClicked);
    
    auto* saveAsButton = new QPushButton(tr("Save As..."), m_generalTab);
    saveAsButton->setToolTip(tr("Save configuration to a new file"));
    connect(saveAsButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnSaveConfigAsClicked);

    pathLayout->addWidget(pathLabel);
    pathLayout->addWidget(m_configPathLabel, 1);
    pathLayout->addWidget(openButton);
    pathLayout->addWidget(loadButton);
    pathLayout->addWidget(saveAsButton);

    layout->addLayout(pathLayout);

    auto* form = new QFormLayout();
    m_xmlRootEdit = new QLineEdit(m_generalTab);
    m_xmlEventEdit = new QLineEdit(m_generalTab);
    m_typeFilterFieldEdit = new QLineEdit(m_generalTab);
    m_typeFilterFieldEdit->setPlaceholderText(tr("e.g., type, level, severity"));
    m_logLevelCombo = new QComboBox(m_generalTab);

    m_logLevelCombo->addItems(
        {"trace", "debug", "info", "warning", "error", "critical", "off"});

    connect(m_logLevelCombo, &QComboBox::currentIndexChanged, this,
        &StructuredConfigDialog::OnLogLevelChanged);

    form->addRow(tr("xmlRootElement"), m_xmlRootEdit);
    form->addRow(tr("xmlEventElement"), m_xmlEventEdit);
    form->addRow(tr("Type Filter Field"), m_typeFilterFieldEdit);
    form->addRow(tr("logLevel"), m_logLevelCombo);
    
    // Dictionary file configuration
    auto* dictionaryLayout = new QHBoxLayout();
    m_dictionaryFileEdit = new QLineEdit(m_generalTab);
    m_dictionaryFileEdit->setPlaceholderText(tr("Path to field_dictionary.json"));
    m_dictionaryFileEdit->setReadOnly(true);
    auto* browseDictButton = new QPushButton(tr("Browse..."), m_generalTab);
    connect(browseDictButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnBrowseDictionaryFileClicked);
    dictionaryLayout->addWidget(m_dictionaryFileEdit);
    dictionaryLayout->addWidget(browseDictButton);
    form->addRow(tr("Dictionary File"), dictionaryLayout);

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

void StructuredConfigDialog::InitDictionaryTab()
{
    m_dictionaryTab = new QWidget(this);
    auto* layout = new QVBoxLayout(m_dictionaryTab);

    m_dictionaryTable = new QTableWidget(m_dictionaryTab);
    m_dictionaryTable->setColumnCount(3);
    QStringList headers;
    headers << tr("Key") << tr("Conversion") << tr("Tooltip Template");
    m_dictionaryTable->setHorizontalHeaderLabels(headers);
    m_dictionaryTable->horizontalHeader()->setStretchLastSection(true);
    m_dictionaryTable->verticalHeader()->setVisible(false);
    m_dictionaryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dictionaryTable->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(m_dictionaryTable, &QTableWidget::itemSelectionChanged, this,
        &StructuredConfigDialog::OnDictionarySelectionChanged);

    layout->addWidget(m_dictionaryTable, 1);

    auto* detailGroup = new QGroupBox(tr("Dictionary Entry Details"), m_dictionaryTab);
    auto* detailLayout = new QFormLayout(detailGroup);

    m_dictKeyEdit = new QLineEdit(detailGroup);
    detailLayout->addRow(tr("Key"), m_dictKeyEdit);

    m_dictConversionCombo = new QComboBox(detailGroup);
    // Populate conversion types from plugin registry
    auto& registry = config::FieldConversionPluginRegistry::GetInstance();
    auto conversions = registry.GetAvailableConversions();
    QStringList conversionItems;
    conversionItems << "tooltip_only"; // Always include tooltip_only
    for (const auto& conv : conversions)
    {
        conversionItems << QString::fromStdString(conv);
    }
    m_dictConversionCombo->addItems(conversionItems);
    detailLayout->addRow(tr("Conversion Type"), m_dictConversionCombo);
    
    connect(m_dictConversionCombo, &QComboBox::currentTextChanged, this, 
        [this](const QString& text) {
            // Show/hide value map table based on conversion type
            bool isValueMap = (text == "value_map");
            if (m_dictValueMapTable)
                m_dictValueMapTable->setVisible(isValueMap);
            if (m_addDictValueButton)
                m_addDictValueButton->setVisible(isValueMap);
            if (m_removeDictValueButton)
                m_removeDictValueButton->setVisible(isValueMap);
        });

    m_dictTooltipEdit = new QLineEdit(detailGroup);
    m_dictTooltipEdit->setPlaceholderText(tr("Use {original} and {converted} as placeholders"));
    detailLayout->addRow(tr("Tooltip Template"), m_dictTooltipEdit);

    // Value map section (only visible for value_map conversion type)
    auto* valueMapLabel = new QLabel(tr("Value Mappings:"), detailGroup);
    detailLayout->addRow(valueMapLabel);
    
    m_dictValueMapTable = new QTableWidget(detailGroup);
    m_dictValueMapTable->setColumnCount(2);
    QStringList vmHeaders;
    vmHeaders << tr("From") << tr("To");
    m_dictValueMapTable->setHorizontalHeaderLabels(vmHeaders);
    m_dictValueMapTable->horizontalHeader()->setStretchLastSection(true);
    m_dictValueMapTable->verticalHeader()->setVisible(false);
    m_dictValueMapTable->setMaximumHeight(150);
    detailLayout->addRow(m_dictValueMapTable);
    
    auto* vmButtonRow = new QHBoxLayout();
    m_addDictValueButton = new QPushButton(tr("Add Mapping"), detailGroup);
    m_removeDictValueButton = new QPushButton(tr("Remove Selected"), detailGroup);
    connect(m_addDictValueButton, &QPushButton::clicked, this, [this]() {
        int row = m_dictValueMapTable->rowCount();
        m_dictValueMapTable->insertRow(row);
        m_dictValueMapTable->setItem(row, 0, new QTableWidgetItem(""));
        m_dictValueMapTable->setItem(row, 1, new QTableWidgetItem(""));
    });
    connect(m_removeDictValueButton, &QPushButton::clicked, this, [this]() {
        int row = m_dictValueMapTable->currentRow();
        if (row >= 0)
            m_dictValueMapTable->removeRow(row);
    });
    vmButtonRow->addWidget(m_addDictValueButton);
    vmButtonRow->addWidget(m_removeDictValueButton);
    vmButtonRow->addStretch();
    detailLayout->addRow(vmButtonRow);
    
    // Initially hide value map controls
    m_dictValueMapTable->setVisible(false);
    m_addDictValueButton->setVisible(false);
    m_removeDictValueButton->setVisible(false);

    m_applyDictionaryChangesButton = new QPushButton(tr("Apply Changes"), detailGroup);
    connect(m_applyDictionaryChangesButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnApplyDictionaryChanges);
    detailLayout->addRow(m_applyDictionaryChangesButton);

    layout->addWidget(detailGroup);

    auto* fileButtonRow = new QHBoxLayout();
    m_loadDictionaryFileButton = new QPushButton(tr("Load Dictionary..."), m_dictionaryTab);
    m_saveDictionaryFileButton = new QPushButton(tr("Save Dictionary..."), m_dictionaryTab);
    connect(m_loadDictionaryFileButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnLoadDictionaryFile);
    connect(m_saveDictionaryFileButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnSaveDictionaryFile);
    fileButtonRow->addWidget(m_loadDictionaryFileButton);
    fileButtonRow->addWidget(m_saveDictionaryFileButton);
    fileButtonRow->addStretch();
    layout->addLayout(fileButtonRow);

    auto* buttonRow = new QHBoxLayout();
    m_addDictionaryButton = new QPushButton(tr("Add Entry"), m_dictionaryTab);
    m_removeDictionaryButton = new QPushButton(tr("Remove Selected"), m_dictionaryTab);
    connect(m_addDictionaryButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnAddDictionary);
    connect(m_removeDictionaryButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnRemoveDictionary);
    buttonRow->addWidget(m_addDictionaryButton);
    buttonRow->addWidget(m_removeDictionaryButton);
    buttonRow->addStretch();

    layout->addLayout(buttonRow);

    m_tabs->addTab(m_dictionaryTab, tr("Dictionary"));
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

    // Column combo will be populated in LoadConfigToUi
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

// AI configuration removed - plugins now manage their own config
// Use the AI Configuration dock widget provided by the active AI plugin

void StructuredConfigDialog::LoadConfigToUi()
{
    auto& cfg = config::GetConfig();

    const auto& path = cfg.GetConfigFilePath();
    m_configPathLabel->setText(QString::fromStdString(path));

    m_xmlRootEdit->setText(QString::fromStdString(cfg.xmlRootElement));
    m_xmlEventEdit->setText(QString::fromStdString(cfg.xmlEventElement));
    m_typeFilterFieldEdit->setText(QString::fromStdString(cfg.typeFilterField));

    const QString logLevel = QString::fromStdString(cfg.logLevel);
    const int idx = m_logLevelCombo->findText(logLevel);
    if (idx >= 0)
        m_logLevelCombo->setCurrentIndex(idx);
    else
        m_logLevelCombo->setCurrentText(QString::fromLatin1("info"));
    
    // Load dictionary file path
    m_dictionaryFileEdit->setText(QString::fromStdString(cfg.GetDictionaryFilePath()));
    
    // Ensure dictionary file is loaded
    const std::string& dictPath = cfg.GetDictionaryFilePath();
    if (!dictPath.empty() && QFile::exists(QString::fromStdString(dictPath)))
    {
        cfg.GetMutableFieldTranslator().LoadFromFile(dictPath);
    }

    // Note: AI configuration is now managed by plugins via their own config panels

    // Populate color column combo with available columns
    m_colorColumnCombo->clear();
    m_colorColumnCombo->addItem(QString::fromStdString(cfg.typeFilterField));
    for (const auto& col : cfg.columns)
    {
        const QString colName = QString::fromStdString(col.name);
        if (colName != QString::fromStdString(cfg.typeFilterField))
        {
            m_colorColumnCombo->addItem(colName);
        }
    }
    // Add any columns from existing color config that aren't in the columns list
    for (const auto& [colName, mappings] : cfg.columnColors)
    {
        const QString qColName = QString::fromStdString(colName);
        if (m_colorColumnCombo->findText(qColName) == -1)
        {
            m_colorColumnCombo->addItem(qColName);
        }
    }
    // Default to typeFilterField if it exists in the combo
    const int typeFilterIdx = m_colorColumnCombo->findText(QString::fromStdString(cfg.typeFilterField));
    if (typeFilterIdx >= 0)
        m_colorColumnCombo->setCurrentIndex(typeFilterIdx);

    RefreshColumnsList();
    RefreshDictionaryList();
    RefreshColorMappings();
    RefreshItemHighlights();
}

void StructuredConfigDialog::OnSaveClicked()
{
    auto& cfg = config::GetConfig();

    cfg.xmlRootElement = m_xmlRootEdit->text().toStdString();
    cfg.xmlEventElement = m_xmlEventEdit->text().toStdString();
    cfg.typeFilterField = m_typeFilterFieldEdit->text().toStdString();
    cfg.logLevel = m_logLevelCombo->currentText().toStdString();
    
    // Save dictionary file path
    cfg.SetDictionaryFilePath(m_dictionaryFileEdit->text().toStdString());
    
    // Save dictionary entries to file FIRST - abort if this fails
    const std::string& dictPath = cfg.GetDictionaryFilePath();
    if (!dictPath.empty())
    {
        util::Logger::Info("Saving dictionary to: {}", dictPath);
        if (!cfg.GetMutableFieldTranslator().SaveToFile(dictPath))
        {
            util::Logger::Error("Failed to save dictionary file: {}", dictPath);
            QMessageBox::critical(this, tr("Dictionary Save Failed"),
                tr("Failed to save dictionary to file:\n%1\n\nConfiguration changes were NOT saved.\nPlease check the file path and permissions.").arg(QString::fromStdString(dictPath)));
            return; // Abort save and keep dialog open
        }
        else
        {
            util::Logger::Info("Dictionary saved successfully to: {}", dictPath);
        }
    }
    else
    {
        util::Logger::Warn("No dictionary file path configured, skipping dictionary save");
    }
    
    // Note: AI configuration is now managed by plugins

    try
    {
        cfg.SaveConfig();
        // Don't reload after save - it would discard any in-memory changes
        // that were made through the UI but haven't been persisted yet
        NotifyObservers();
        QMessageBox::information(this, tr("Config"),
            tr("Configuration and dictionary saved successfully."));
        accept(); // Only close dialog after everything saved successfully
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

void StructuredConfigDialog::OnLoadConfigClicked()
{
    QFileDialog dialog(this, tr("Load Configuration File"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    dialog.setDirectory(QString::fromStdString(config::GetConfig().GetConfigFilePath()));
    dialog.setNameFilter(tr("JSON Files (*.json);;All Files (*)"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    
    if (dialog.exec() != QDialog::Accepted)
        return;
    
    QString fileName = dialog.selectedFiles().value(0);
    if (fileName.isEmpty())
        return;
    
    try
    {
        auto& cfg = config::GetConfig();
        cfg.SetConfigFilePath(fileName.toStdString());
        cfg.LoadConfig();
        
        // Reload dialog with new config
        LoadConfigToUi();
        
        util::Logger::Info("Loaded configuration from: {}", fileName.toStdString());
        
        QMessageBox::information(this, tr("Config Loaded"),
            tr("Configuration loaded successfully from:\n%1\n\n"
               "The dialog has been refreshed with the loaded settings.")
                .arg(fileName));
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Failed to load config from {}: {}", 
            fileName.toStdString(), ex.what());
        QMessageBox::critical(this, tr("Load Config Failed"), 
            tr("Failed to load configuration:\n%1").arg(ex.what()));
    }
}

void StructuredConfigDialog::OnBrowseDictionaryFileClicked()
{
    QFileDialog dialog(this, tr("Select Dictionary File"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    
    // Start from current dictionary file directory or config directory
    QString currentPath = m_dictionaryFileEdit->text();
    if (currentPath.isEmpty() || !QFile::exists(currentPath))
    {
        currentPath = QString::fromStdString(config::GetConfig().GetConfigFilePath());
    }
    
    dialog.setDirectory(QFileInfo(currentPath).absolutePath());
    dialog.setNameFilter(tr("JSON Files (*.json);;All Files (*)"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    
    if (dialog.exec() != QDialog::Accepted)
        return;
    
    QString fileName = dialog.selectedFiles().value(0);
    if (fileName.isEmpty())
        return;
    
    m_dictionaryFileEdit->setText(fileName);
}

void StructuredConfigDialog::OnSaveConfigAsClicked()
{
    QFileDialog dialog(this, tr("Save Configuration As"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    dialog.setDirectory(QString::fromStdString(config::GetConfig().GetConfigFilePath()));
    dialog.setNameFilter(tr("JSON Files (*.json);;All Files (*)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix("json");
    
    if (dialog.exec() != QDialog::Accepted)
        return;
    
    QString fileName = dialog.selectedFiles().value(0);
    if (fileName.isEmpty())
        return;
    
    try
    {
        auto& cfg = config::GetConfig();
        
        // Save current dialog state to config first
        OnSaveClicked();
        
        // Then save to new location
        cfg.SetConfigFilePath(fileName.toStdString());
        cfg.SaveConfig();
        
        // Update the displayed path
        m_configPathLabel->setText(QString::fromStdString(fileName.toStdString()));
        
        util::Logger::Info("Saved configuration to: {}", fileName.toStdString());
        
        QMessageBox::information(this, tr("Config Saved"),
            tr("Configuration saved successfully to:\n%1\n\n"
               "The application will now use this configuration file.")
                .arg(fileName));
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Failed to save config to {}: {}", 
            fileName.toStdString(), ex.what());
        QMessageBox::critical(this, tr("Save Config Failed"), 
            tr("Failed to save configuration:\n%1").arg(ex.what()));
    }
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

// AI configuration handlers removed - plugins now manage their own config

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

void StructuredConfigDialog::RefreshItemHighlights()
{
    m_itemHighlightsTable->setRowCount(0);
    
    const auto& itemHighlights = config::GetConfig().itemHighlights;
    
    for (const auto& [key, highlight] : itemHighlights)
    {
        const int row = m_itemHighlightsTable->rowCount();
        m_itemHighlightsTable->insertRow(row);
        
        m_itemHighlightsTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(key)));
        m_itemHighlightsTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(highlight.backgroundColor)));
        m_itemHighlightsTable->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(highlight.foregroundColor)));
        m_itemHighlightsTable->setItem(row, 3, new QTableWidgetItem(highlight.bold ? tr("Yes") : tr("No")));
        m_itemHighlightsTable->setItem(row, 4, new QTableWidgetItem(highlight.italic ? tr("Yes") : tr("No")));
    }
    
    m_selectedItemHighlightRow = -1;
}

void StructuredConfigDialog::OnItemHighlightSelectionChanged()
{
    const auto selected = m_itemHighlightsTable->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        m_selectedItemHighlightRow = -1;
        m_itemKeyEdit->clear();
        m_itemBoldCheck->setChecked(false);
        m_itemItalicCheck->setChecked(false);
        m_itemBgColor = QColor(Qt::white);
        m_itemFgColor = QColor(Qt::black);
        UpdateItemHighlightSwatches();
        UpdateItemHighlightPreview();
        return;
    }
    
    const int row = selected.first().row();
    m_selectedItemHighlightRow = row;
    UpdateItemHighlightEditorsFromSelection(row);
}

void StructuredConfigDialog::UpdateItemHighlightEditorsFromSelection(int row)
{
    if (row < 0 || row >= m_itemHighlightsTable->rowCount())
        return;
    
    auto* keyItem = m_itemHighlightsTable->item(row, 0);
    auto* bgItem = m_itemHighlightsTable->item(row, 1);
    auto* fgItem = m_itemHighlightsTable->item(row, 2);
    auto* boldItem = m_itemHighlightsTable->item(row, 3);
    auto* italicItem = m_itemHighlightsTable->item(row, 4);
    
    if (keyItem)
        m_itemKeyEdit->setText(keyItem->text());
    
    if (bgItem)
        m_itemBgColor = HexToColor(bgItem->text());
    else
        m_itemBgColor = QColor(Qt::white);
    
    if (fgItem)
        m_itemFgColor = HexToColor(fgItem->text());
    else
        m_itemFgColor = QColor(Qt::black);
    
    if (boldItem)
        m_itemBoldCheck->setChecked(boldItem->text() == tr("Yes"));
    
    if (italicItem)
        m_itemItalicCheck->setChecked(italicItem->text() == tr("Yes"));
    
    UpdateItemHighlightSwatches();
    UpdateItemHighlightPreview();
}

void StructuredConfigDialog::OnItemBgColorButton()
{
    QColor color = QColorDialog::getColor(m_itemBgColor, this, tr("Choose Background Color"));
    if (color.isValid())
    {
        m_itemBgColor = color;
        UpdateItemHighlightSwatches();
        UpdateItemHighlightPreview();
    }
}

void StructuredConfigDialog::OnItemFgColorButton()
{
    QColor color = QColorDialog::getColor(m_itemFgColor, this, tr("Choose Text Color"));
    if (color.isValid())
    {
        m_itemFgColor = color;
        UpdateItemHighlightSwatches();
        UpdateItemHighlightPreview();
    }
}

void StructuredConfigDialog::OnItemDefaultBgColor()
{
    m_itemBgColor = QColor(Qt::white);
    UpdateItemHighlightSwatches();
    UpdateItemHighlightPreview();
}

void StructuredConfigDialog::OnItemDefaultFgColor()
{
    m_itemFgColor = QColor(Qt::black);
    UpdateItemHighlightSwatches();
    UpdateItemHighlightPreview();
}

void StructuredConfigDialog::OnAddItemHighlight()
{
    const QString key = m_itemKeyEdit->text().trimmed();
    if (key.isEmpty())
    {
        QMessageBox::information(this, tr("Missing Key"),
            tr("Please enter a key name for the highlight."));
        return;
    }
    
    auto& itemHighlights = config::GetConfig().itemHighlights;
    
    if (itemHighlights.find(key.toStdString()) != itemHighlights.end())
    {
        QMessageBox::information(this, tr("Key Exists"),
            tr("A highlight for this key already exists. Use Update instead."));
        return;
    }
    
    config::ItemHighlight highlight;
    highlight.backgroundColor = ColorToHex(m_itemBgColor).toStdString();
    highlight.foregroundColor = ColorToHex(m_itemFgColor).toStdString();
    highlight.bold = m_itemBoldCheck->isChecked();
    highlight.italic = m_itemItalicCheck->isChecked();
    
    itemHighlights[key.toStdString()] = highlight;
    
    RefreshItemHighlights();
}

void StructuredConfigDialog::OnUpdateItemHighlight()
{
    const QString key = m_itemKeyEdit->text().trimmed();
    if (key.isEmpty())
    {
        QMessageBox::information(this, tr("Missing Key"),
            tr("Please enter a key name for the highlight."));
        return;
    }
    
    auto& itemHighlights = config::GetConfig().itemHighlights;
    
    config::ItemHighlight highlight;
    highlight.backgroundColor = ColorToHex(m_itemBgColor).toStdString();
    highlight.foregroundColor = ColorToHex(m_itemFgColor).toStdString();
    highlight.bold = m_itemBoldCheck->isChecked();
    highlight.italic = m_itemItalicCheck->isChecked();
    
    itemHighlights[key.toStdString()] = highlight;
    
    RefreshItemHighlights();
}

void StructuredConfigDialog::OnDeleteItemHighlight()
{
    const auto selected = m_itemHighlightsTable->selectionModel()->selectedRows();
    if (selected.isEmpty())
    {
        QMessageBox::information(this, tr("No Selection"),
            tr("Please select an item highlight to delete."));
        return;
    }
    
    const int row = selected.first().row();
    auto* keyItem = m_itemHighlightsTable->item(row, 0);
    if (!keyItem)
        return;
    
    const QString key = keyItem->text();
    
    const auto answer = QMessageBox::question(this, tr("Confirm Deletion"),
        tr("Are you sure you want to delete the highlight for '%1'?").arg(key));
    if (answer != QMessageBox::Yes)
        return;
    
    auto& itemHighlights = config::GetConfig().itemHighlights;
    itemHighlights.erase(key.toStdString());
    
    RefreshItemHighlights();
}

void StructuredConfigDialog::UpdateItemHighlightSwatches()
{
    if (m_itemBgColorSwatch)
    {
        m_itemBgColorSwatch->setStyleSheet(
            QString::fromLatin1("background-color: %1;")
                .arg(ColorToHex(m_itemBgColor)));
    }
    if (m_itemFgColorSwatch)
    {
        m_itemFgColorSwatch->setStyleSheet(
            QString::fromLatin1("background-color: %1;")
                .arg(ColorToHex(m_itemFgColor)));
    }
}

void StructuredConfigDialog::UpdateItemHighlightPreview()
{
    if (!m_itemPreviewPanel || !m_itemPreviewLabel)
        return;
    
    m_itemPreviewPanel->setStyleSheet(
        QString::fromLatin1("background-color: %1;")
            .arg(ColorToHex(m_itemBgColor)));
    m_itemPreviewLabel->setStyleSheet(
        QString::fromLatin1("color: %1;")
            .arg(ColorToHex(m_itemFgColor)));
    
    QFont font = m_itemPreviewLabel->font();
    font.setBold(m_itemBoldCheck->isChecked());
    font.setItalic(m_itemItalicCheck->isChecked());
    m_itemPreviewLabel->setFont(font);
    
    const QString key = m_itemKeyEdit->text().trimmed();
    m_itemPreviewLabel->setText(key.isEmpty() ? tr("Sample Text") : key);
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

void StructuredConfigDialog::RefreshDictionaryList()
{
    m_dictionaryTable->setRowCount(0);
    
    auto& cfg = config::GetConfig();
    const auto& translations = cfg.GetMutableFieldTranslator().GetAllTranslations();
    
    int row = 0;
    for (const auto& [key, trans] : translations)
    {
        m_dictionaryTable->insertRow(row);
        m_dictionaryTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(trans.key)));
        m_dictionaryTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(trans.conversionType)));
        m_dictionaryTable->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(trans.tooltipTemplate)));
        ++row;
    }
}

void StructuredConfigDialog::LoadSelectedDictionaryToEditors()
{
    int row = m_dictionaryTable->currentRow();
    if (row < 0)
        return;
    
    auto* keyItem = m_dictionaryTable->item(row, 0);
    if (!keyItem)
        return;
    
    std::string key = keyItem->text().toStdString();
    
    auto& cfg = config::GetConfig();
    const auto& translations = cfg.GetMutableFieldTranslator().GetAllTranslations();
    auto it = translations.find(key);
    
    if (it == translations.end())
        return;
    
    const auto& trans = it->second;
    
    m_dictKeyEdit->setText(QString::fromStdString(trans.key));
    
    int convIdx = m_dictConversionCombo->findText(QString::fromStdString(trans.conversionType));
    if (convIdx >= 0)
        m_dictConversionCombo->setCurrentIndex(convIdx);
    
    m_dictTooltipEdit->setText(QString::fromStdString(trans.tooltipTemplate));
    
    // Load value map
    m_dictValueMapTable->setRowCount(0);
    int vmRow = 0;
    for (const auto& [from, to] : trans.valueMap)
    {
        m_dictValueMapTable->insertRow(vmRow);
        m_dictValueMapTable->setItem(vmRow, 0, new QTableWidgetItem(QString::fromStdString(from)));
        m_dictValueMapTable->setItem(vmRow, 1, new QTableWidgetItem(QString::fromStdString(to)));
        ++vmRow;
    }
}

void StructuredConfigDialog::OnDictionarySelectionChanged()
{
    m_selectedDictionaryRow = m_dictionaryTable->currentRow();
    if (m_selectedDictionaryRow >= 0)
    {
        LoadSelectedDictionaryToEditors();
    }
}

void StructuredConfigDialog::OnAddDictionary()
{
    config::FieldDictionary newTrans;
    newTrans.key = "new_field";
    newTrans.conversionType = "hex_to_ascii";
    newTrans.tooltipTemplate = "Hex: {original}\\nASCII: {converted}";
    
    auto& cfg = config::GetConfig();
    cfg.GetMutableFieldTranslator().SetTranslation(newTrans);
    
    // Save dictionary to file immediately
    const std::string& dictPath = cfg.GetDictionaryFilePath();
    if (!dictPath.empty())
    {
        if (cfg.GetMutableFieldTranslator().SaveToFile(dictPath))
        {
            util::Logger::Info("Dictionary saved to: {}", dictPath);
        }
        else
        {
            QMessageBox::warning(this, tr("Save Failed"),
                tr("Failed to save dictionary changes to file:\n%1")
                .arg(QString::fromStdString(dictPath)));
        }
    }
    
    RefreshDictionaryList();
    
    // Select the new entry
    for (int i = 0; i < m_dictionaryTable->rowCount(); ++i)
    {
        auto* item = m_dictionaryTable->item(i, 0);
        if (item && item->text() == "new_field")
        {
            m_dictionaryTable->selectRow(i);
            break;
        }
    }
}

void StructuredConfigDialog::OnRemoveDictionary()
{
    int row = m_dictionaryTable->currentRow();
    if (row < 0)
        return;
    
    auto* keyItem = m_dictionaryTable->item(row, 0);
    if (!keyItem)
        return;
    
    std::string key = keyItem->text().toStdString();
    
    auto reply = QMessageBox::question(this, tr("Remove Dictionary Entry"),
        tr("Are you sure you want to remove the dictionary entry for '%1'?")
            .arg(QString::fromStdString(key)),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes)
    {
        auto& cfg = config::GetConfig();
        cfg.GetMutableFieldTranslator().RemoveTranslation(key);
        
        // Save dictionary to file immediately
        const std::string& dictPath = cfg.GetDictionaryFilePath();
        if (!dictPath.empty())
        {
            if (cfg.GetMutableFieldTranslator().SaveToFile(dictPath))
            {
                util::Logger::Info("Dictionary saved to: {}", dictPath);
            }
            else
            {
                QMessageBox::warning(this, tr("Save Failed"),
                    tr("Failed to save dictionary changes to file:\n%1")
                    .arg(QString::fromStdString(dictPath)));
            }
        }
        
        RefreshDictionaryList();
    }
}

void StructuredConfigDialog::OnApplyDictionaryChanges()
{
    int row = m_dictionaryTable->currentRow();
    if (row < 0)
        return;
    
    auto* keyItem = m_dictionaryTable->item(row, 0);
    if (!keyItem)
        return;
    
    std::string oldKey = keyItem->text().toStdString();
    std::string newKey = m_dictKeyEdit->text().toStdString();
    
    if (newKey.empty())
    {
        QMessageBox::warning(this, tr("Invalid Key"),
            tr("Key cannot be empty."));
        return;
    }
    
    // Check if key changed and new key already exists
    if (oldKey != newKey)
    {
        const auto& translations = config::GetConfig().GetMutableFieldTranslator().GetAllTranslations();
        if (translations.find(newKey) != translations.end())
        {
            QMessageBox::warning(this, tr("Duplicate Key"),
                tr("A dictionary entry with this key already exists."));
            return;
        }
    }
    
    config::FieldDictionary trans;
    trans.key = newKey;
    trans.conversionType = m_dictConversionCombo->currentText().toStdString();
    trans.tooltipTemplate = m_dictTooltipEdit->text().toStdString();
    
    // Collect value map
    for (int i = 0; i < m_dictValueMapTable->rowCount(); ++i)
    {
        auto* fromItem = m_dictValueMapTable->item(i, 0);
        auto* toItem = m_dictValueMapTable->item(i, 1);
        if (fromItem && toItem)
        {
            std::string from = fromItem->text().toStdString();
            std::string to = toItem->text().toStdString();
            if (!from.empty())
            {
                trans.valueMap[from] = to;
            }
        }
    }
    
    auto& cfg = config::GetConfig();
    
    // If key changed, remove old entry
    if (oldKey != newKey)
    {
        cfg.GetMutableFieldTranslator().RemoveTranslation(oldKey);
    }
    
    cfg.GetMutableFieldTranslator().SetTranslation(trans);
    
    // Save dictionary to file immediately
    const std::string& dictPath = cfg.GetDictionaryFilePath();
    if (!dictPath.empty())
    {
        if (cfg.GetMutableFieldTranslator().SaveToFile(dictPath))
        {
            util::Logger::Info("Dictionary saved to: {}", dictPath);
        }
        else
        {
            QMessageBox::warning(this, tr("Save Failed"),
                tr("Failed to save dictionary changes to file:\n%1")
                .arg(QString::fromStdString(dictPath)));
        }
    }
    
    RefreshDictionaryList();
    
    // Reselect the row
    for (int i = 0; i < m_dictionaryTable->rowCount(); ++i)
    {
        auto* item = m_dictionaryTable->item(i, 0);
        if (item && item->text() == QString::fromStdString(newKey))
        {
            m_dictionaryTable->selectRow(i);
            break;
        }
    }
}

void StructuredConfigDialog::OnLoadDictionaryFile()
{
    QFileDialog dialog(this, tr("Load Dictionary File"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    
    QString currentPath = m_dictionaryFileEdit->text();
    if (currentPath.isEmpty() || !QFile::exists(currentPath))
    {
        currentPath = QString::fromStdString(config::GetConfig().GetConfigFilePath());
    }
    
    dialog.setDirectory(QFileInfo(currentPath).absolutePath());
    dialog.setNameFilter(tr("JSON Files (*.json);;All Files (*)"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    
    if (dialog.exec() != QDialog::Accepted)
        return;
    
    QString fileName = dialog.selectedFiles().value(0);
    if (fileName.isEmpty())
        return;
    
    // Load dictionary from selected file
    auto& cfg = config::GetConfig();
    if (cfg.GetMutableFieldTranslator().LoadFromFile(fileName.toStdString()))
    {
        m_dictionaryFileEdit->setText(fileName);
        cfg.SetDictionaryFilePath(fileName.toStdString());
        RefreshDictionaryList();
        
        QMessageBox::information(this, tr("Dictionary Loaded"),
            tr("Dictionary loaded successfully from:\n%1").arg(fileName));
    }
    else
    {
        QMessageBox::critical(this, tr("Load Failed"),
            tr("Failed to load dictionary from:\n%1").arg(fileName));
    }
}

void StructuredConfigDialog::OnSaveDictionaryFile()
{
    QFileDialog dialog(this, tr("Save Dictionary File"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    
    QString currentPath = m_dictionaryFileEdit->text();
    if (currentPath.isEmpty())
    {
        currentPath = QString::fromStdString(config::GetConfig().GetDictionaryFilePath());
    }
    
    dialog.setDirectory(QFileInfo(currentPath).absolutePath());
    dialog.setNameFilter(tr("JSON Files (*.json);;All Files (*)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix("json");
    
    if (!currentPath.isEmpty())
    {
        dialog.selectFile(currentPath);
    }
    
    if (dialog.exec() != QDialog::Accepted)
        return;
    
    QString fileName = dialog.selectedFiles().value(0);
    if (fileName.isEmpty())
        return;
    
    // Save dictionary to selected file
    auto& cfg = config::GetConfig();
    if (cfg.GetMutableFieldTranslator().SaveToFile(fileName.toStdString()))
    {
        m_dictionaryFileEdit->setText(fileName);
        cfg.SetDictionaryFilePath(fileName.toStdString());
        
        QMessageBox::information(this, tr("Dictionary Saved"),
            tr("Dictionary saved successfully to:\n%1").arg(fileName));
    }
    else
    {
        QMessageBox::critical(this, tr("Save Failed"),
            tr("Failed to save dictionary to:\n%1").arg(fileName));
    }
}

void StructuredConfigDialog::InitItemHighlightsTab()
{
    m_itemHighlightsTab = new QWidget(this);
    auto* layout = new QVBoxLayout(m_itemHighlightsTab);

    layout->addWidget(new QLabel(tr("Configure highlighting for item keys in the Item Details View:"), m_itemHighlightsTab));

    m_itemHighlightsTable = new QTableWidget(m_itemHighlightsTab);
    m_itemHighlightsTable->setColumnCount(5);
    QStringList headers;
    headers << tr("Key Name") << tr("Background") << tr("Foreground") << tr("Bold") << tr("Italic");
    m_itemHighlightsTable->setHorizontalHeaderLabels(headers);
    m_itemHighlightsTable->horizontalHeader()->setStretchLastSection(true);
    m_itemHighlightsTable->verticalHeader()->setVisible(false);
    m_itemHighlightsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_itemHighlightsTable->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(m_itemHighlightsTable, &QTableWidget::itemSelectionChanged, this,
        &StructuredConfigDialog::OnItemHighlightSelectionChanged);

    layout->addWidget(m_itemHighlightsTable, 1);

    auto* editGroup = new QGroupBox(tr("Add/Edit Item Highlight"), m_itemHighlightsTab);
    auto* editLayout = new QFormLayout(editGroup);

    m_itemKeyEdit = new QLineEdit(editGroup);
    editLayout->addRow(tr("Key Name"), m_itemKeyEdit);

    // Background controls
    auto* bgRow = new QHBoxLayout();
    m_itemBgColorSwatch = new QFrame(editGroup);
    m_itemBgColorSwatch->setFrameShape(QFrame::Box);
    m_itemBgColorSwatch->setFixedSize(30, 20);
    m_itemBgColorButton = new QPushButton(tr("Choose..."), editGroup);
    m_itemDefaultBgButton = new QPushButton(tr("Default"), editGroup);
    bgRow->addWidget(m_itemBgColorSwatch);
    bgRow->addWidget(m_itemBgColorButton);
    bgRow->addWidget(m_itemDefaultBgButton);

    connect(m_itemBgColorButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnItemBgColorButton);
    connect(m_itemDefaultBgButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnItemDefaultBgColor);

    editLayout->addRow(tr("Background"), bgRow);

    // Foreground controls
    auto* fgRow = new QHBoxLayout();
    m_itemFgColorSwatch = new QFrame(editGroup);
    m_itemFgColorSwatch->setFrameShape(QFrame::Box);
    m_itemFgColorSwatch->setFixedSize(30, 20);
    m_itemFgColorButton = new QPushButton(tr("Choose..."), editGroup);
    m_itemDefaultFgButton = new QPushButton(tr("Default"), editGroup);
    fgRow->addWidget(m_itemFgColorSwatch);
    fgRow->addWidget(m_itemFgColorButton);
    fgRow->addWidget(m_itemDefaultFgButton);

    connect(m_itemFgColorButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnItemFgColorButton);
    connect(m_itemDefaultFgButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnItemDefaultFgColor);

    editLayout->addRow(tr("Foreground"), fgRow);

    // Font style checkboxes
    auto* styleRow = new QHBoxLayout();
    m_itemBoldCheck = new QCheckBox(tr("Bold"), editGroup);
    m_itemItalicCheck = new QCheckBox(tr("Italic"), editGroup);
    styleRow->addWidget(m_itemBoldCheck);
    styleRow->addWidget(m_itemItalicCheck);
    styleRow->addStretch();

    editLayout->addRow(tr("Font Style"), styleRow);

    // Preview
    m_itemPreviewPanel = new QFrame(editGroup);
    m_itemPreviewPanel->setFrameShape(QFrame::StyledPanel);
    auto* previewLayout = new QVBoxLayout(m_itemPreviewPanel);
    m_itemPreviewLabel = new QLabel(tr("Sample Text"), m_itemPreviewPanel);
    m_itemPreviewLabel->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_itemPreviewLabel);

    editLayout->addRow(tr("Preview"), m_itemPreviewPanel);

    layout->addWidget(editGroup);

    auto* buttonRow = new QHBoxLayout();
    m_addItemHighlightButton = new QPushButton(tr("Add"), m_itemHighlightsTab);
    m_updateItemHighlightButton = new QPushButton(tr("Update"), m_itemHighlightsTab);
    m_deleteItemHighlightButton = new QPushButton(tr("Delete"), m_itemHighlightsTab);
    connect(m_addItemHighlightButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnAddItemHighlight);
    connect(m_updateItemHighlightButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnUpdateItemHighlight);
    connect(m_deleteItemHighlightButton, &QPushButton::clicked, this,
        &StructuredConfigDialog::OnDeleteItemHighlight);

    buttonRow->addWidget(m_addItemHighlightButton);
    buttonRow->addWidget(m_updateItemHighlightButton);
    buttonRow->addWidget(m_deleteItemHighlightButton);
    buttonRow->addStretch();

    layout->addLayout(buttonRow);

    m_tabs->addTab(m_itemHighlightsTab, tr("Item Highlights"));

    // Default colors
    m_itemBgColor = QColor(Qt::white);
    m_itemFgColor = QColor(Qt::black);
    UpdateItemHighlightSwatches();
    UpdateItemHighlightPreview();
}

void StructuredConfigDialog::InitPluginsTab()
{
    // Set plugins directory
    m_pluginsTab = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(m_pluginsTab);

    // Top section: Plugins list
    auto* listGroup = new QGroupBox(tr("Available Plugins"), m_pluginsTab);
    auto* listLayout = new QVBoxLayout(listGroup);

    m_pluginsTable = new QTableWidget(listGroup);
    m_pluginsTable->setColumnCount(6);
    m_pluginsTable->setHorizontalHeaderLabels({
        tr("Name"), tr("ID"), tr("Version"), tr("Type"), tr("Status"), tr("Enabled")
    });
    m_pluginsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pluginsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pluginsTable->horizontalHeader()->setStretchLastSection(true);
    connect(m_pluginsTable, &QTableWidget::itemSelectionChanged,
        this, &StructuredConfigDialog::OnPluginSelectionChanged);
    listLayout->addWidget(m_pluginsTable);

    auto* buttonLayout = new QHBoxLayout();
    m_refreshPluginsButton = new QPushButton(tr("Refresh"), listGroup);
    connect(m_refreshPluginsButton, &QPushButton::clicked,
        this, &StructuredConfigDialog::OnRefreshPluginsClicked);
    buttonLayout->addWidget(m_refreshPluginsButton);
    buttonLayout->addStretch();
    listLayout->addLayout(buttonLayout);

    mainLayout->addWidget(listGroup);

    // Middle section: Plugin details
    auto* detailsGroup = new QGroupBox(tr("Plugin Details"), m_pluginsTab);
    auto* detailsLayout = new QFormLayout(detailsGroup);

    m_pluginIdLabel = new QLabel(detailsGroup);
    m_pluginNameLabel = new QLabel(detailsGroup);
    m_pluginVersionLabel = new QLabel(detailsGroup);
    m_pluginAuthorLabel = new QLabel(detailsGroup);
    m_pluginTypeLabel = new QLabel(detailsGroup);
    m_pluginStatusLabel = new QLabel(detailsGroup);
    m_pluginLicenseLabel = new QLabel(detailsGroup);
    m_pluginPathLabel = new QLabel(detailsGroup);
    m_pluginPathLabel->setWordWrap(true);
    m_pluginPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    detailsLayout->addRow(tr("ID:"), m_pluginIdLabel);
    detailsLayout->addRow(tr("Name:"), m_pluginNameLabel);
    detailsLayout->addRow(tr("Version:"), m_pluginVersionLabel);
    detailsLayout->addRow(tr("Author:"), m_pluginAuthorLabel);
    detailsLayout->addRow(tr("Type:"), m_pluginTypeLabel);
    detailsLayout->addRow(tr("Status:"), m_pluginStatusLabel);
    detailsLayout->addRow(tr("License Required:"), m_pluginLicenseLabel);
    detailsLayout->addRow(tr("Location:"), m_pluginPathLabel);

    mainLayout->addWidget(detailsGroup);

    // Bottom section: Plugin management
    auto* manageGroup = new QGroupBox(tr("Plugin Management"), m_pluginsTab);
    auto* manageLayout = new QVBoxLayout(manageGroup);

    // Load new plugin
    auto* loadLayout = new QHBoxLayout();
    loadLayout->addWidget(new QLabel(tr("Plugin File:"), manageGroup));
    m_pluginPathEdit = new QLineEdit(manageGroup);
    loadLayout->addWidget(m_pluginPathEdit, 1);
    m_browsePluginButton = new QPushButton(tr("Browse..."), manageGroup);
    connect(m_browsePluginButton, &QPushButton::clicked,
        this, &StructuredConfigDialog::OnBrowsePluginClicked);
    loadLayout->addWidget(m_browsePluginButton);
    m_loadPluginButton = new QPushButton(tr("Register Plugin"), manageGroup);
    connect(m_loadPluginButton, &QPushButton::clicked,
        this, &StructuredConfigDialog::OnLoadPluginClicked);
    loadLayout->addWidget(m_loadPluginButton);
    manageLayout->addLayout(loadLayout);

    // License management
    auto* licenseLayout = new QHBoxLayout();
    licenseLayout->addWidget(new QLabel(tr("License Key:"), manageGroup));
    m_pluginLicenseEdit = new QLineEdit(manageGroup);
    m_pluginLicenseEdit->setEchoMode(QLineEdit::Password);
    licenseLayout->addWidget(m_pluginLicenseEdit, 1);
    m_setLicenseButton = new QPushButton(tr("Set License"), manageGroup);
    connect(m_setLicenseButton, &QPushButton::clicked,
        this, &StructuredConfigDialog::OnSetLicenseClicked);
    licenseLayout->addWidget(m_setLicenseButton);
    manageLayout->addLayout(licenseLayout);

    // Plugin controls
    auto* controlLayout = new QHBoxLayout();
    m_autoLoadPluginCheck = new QCheckBox(tr("Auto-load on startup"), manageGroup);
    connect(m_autoLoadPluginCheck, &QCheckBox::toggled,
        this, &StructuredConfigDialog::OnAutoLoadPluginToggled);
    controlLayout->addWidget(m_autoLoadPluginCheck);
    m_enablePluginButton = new QPushButton(tr("Enable/Disable"), manageGroup);
    connect(m_enablePluginButton, &QPushButton::clicked,
        this, &StructuredConfigDialog::OnEnablePluginClicked);
    controlLayout->addWidget(m_enablePluginButton);
    m_unloadPluginButton = new QPushButton(tr("Unregister Plugin"), manageGroup);
    connect(m_unloadPluginButton, &QPushButton::clicked,
        this, &StructuredConfigDialog::OnUnloadPluginClicked);
    controlLayout->addWidget(m_unloadPluginButton);
    controlLayout->addStretch();
    manageLayout->addLayout(controlLayout);

    mainLayout->addWidget(manageGroup);

    m_tabs->addTab(m_pluginsTab, tr("Plugins"));

    // Load initial plugins list
    RefreshPluginsList();
}

void StructuredConfigDialog::RefreshPluginsList()
{
    m_pluginsTable->setRowCount(0);
    
    auto& pluginMgr = plugin::PluginManager::GetInstance();
    const auto& plugins = pluginMgr.GetLoadedPlugins();

    int row = 0;
    for (const auto& [id, info] : plugins)
    {
        if (!info.instance) continue;

        auto metadata = info.instance->GetMetadata();
        
        m_pluginsTable->insertRow(row);
        m_pluginsTable->setItem(row, 0, new QTableWidgetItem(
            QString::fromStdString(metadata.name)));
        m_pluginsTable->setItem(row, 1, new QTableWidgetItem(
            QString::fromStdString(metadata.id)));
        m_pluginsTable->setItem(row, 2, new QTableWidgetItem(
            QString::fromStdString(metadata.version)));
        
        QString typeStr;
        switch (metadata.type)
        {
            case plugin::PluginType::Parser: typeStr = "Parser"; break;
            case plugin::PluginType::Filter: typeStr = "Filter"; break;
            case plugin::PluginType::FieldConversion: typeStr = "Field Conversion"; break;
            case plugin::PluginType::Exporter: typeStr = "Exporter"; break;
            case plugin::PluginType::Analyzer: typeStr = "Analyzer"; break;
            case plugin::PluginType::Connector: typeStr = "Connector"; break;
            case plugin::PluginType::Visualizer: typeStr = "Visualizer"; break;
            case plugin::PluginType::Custom: typeStr = "Custom"; break;
        }
        m_pluginsTable->setItem(row, 3, new QTableWidgetItem(typeStr));
        
        QString statusStr;
        switch (info.instance->GetStatus())
        {
            case plugin::PluginStatus::Unloaded: statusStr = "Unloaded"; break;
            case plugin::PluginStatus::Loaded: statusStr = "Loaded"; break;
            case plugin::PluginStatus::Initialized: statusStr = "Initialized"; break;
            case plugin::PluginStatus::Active: statusStr = "Active"; break;
            case plugin::PluginStatus::Error: statusStr = "Error"; break;
            case plugin::PluginStatus::Disabled: statusStr = "Disabled"; break;
        }
        m_pluginsTable->setItem(row, 4, new QTableWidgetItem(statusStr));
        
        m_pluginsTable->setItem(row, 5, new QTableWidgetItem(
            info.enabled ? tr("Yes") : tr("No")));
        
        row++;
    }
    
    m_pluginsTable->resizeColumnsToContents();
}

void StructuredConfigDialog::UpdatePluginDetails()
{
    int row = m_pluginsTable->currentRow();
    if (row < 0)
    {
        m_pluginIdLabel->clear();
        m_pluginNameLabel->clear();
        m_pluginVersionLabel->clear();
        m_pluginAuthorLabel->clear();
        m_pluginTypeLabel->clear();
        m_pluginStatusLabel->clear();
        m_pluginLicenseLabel->clear();
        m_pluginPathLabel->clear();
        m_autoLoadPluginCheck->setChecked(false);
        m_autoLoadPluginCheck->setEnabled(false);
        m_enablePluginButton->setEnabled(false);
        m_unloadPluginButton->setEnabled(false);
        m_setLicenseButton->setEnabled(false);
        return;
    }

    QString pluginId = m_pluginsTable->item(row, 1)->text();
    auto& pluginMgr = plugin::PluginManager::GetInstance();
    auto* pluginInstance = pluginMgr.GetPlugin(pluginId.toStdString());
    
    if (!pluginInstance)
    {
        UpdatePluginDetails(); // Clear
        return;
    }

    auto metadata = pluginInstance->GetMetadata();
    
    m_pluginIdLabel->setText(QString::fromStdString(metadata.id));
    m_pluginNameLabel->setText(QString::fromStdString(metadata.name));
    m_pluginVersionLabel->setText(QString::fromStdString(metadata.version));
    m_pluginAuthorLabel->setText(QString::fromStdString(metadata.author));
    
    QString typeStr;
    switch (metadata.type)
    {
        case plugin::PluginType::Parser: typeStr = "Parser (IDataParser)"; break;
        case plugin::PluginType::Filter: typeStr = "Filter (IFilterStrategy)"; break;
        case plugin::PluginType::FieldConversion: typeStr = "Field Conversion (IFieldConversionPlugin)"; break;
        case plugin::PluginType::Exporter: typeStr = "Exporter"; break;
        case plugin::PluginType::Analyzer: typeStr = "Analyzer"; break;
        case plugin::PluginType::Connector: typeStr = "Connector"; break;
        case plugin::PluginType::Visualizer: typeStr = "Visualizer"; break;
        case plugin::PluginType::Custom: typeStr = "Custom"; break;
    }
    m_pluginTypeLabel->setText(typeStr);
    
    QString statusStr;
    auto status = pluginInstance->GetStatus();
    switch (status)
    {
        case plugin::PluginStatus::Unloaded: statusStr = "Unloaded"; break;
        case plugin::PluginStatus::Loaded: statusStr = "Loaded"; break;
        case plugin::PluginStatus::Initialized: statusStr = "Initialized"; break;
        case plugin::PluginStatus::Active: statusStr = "Active"; break;
        case plugin::PluginStatus::Error: 
            statusStr = QString("Error: %1").arg(
                QString::fromStdString(pluginInstance->GetLastError())); 
            break;
        case plugin::PluginStatus::Disabled: statusStr = "Disabled"; break;
    }
    m_pluginStatusLabel->setText(statusStr);
    
    m_pluginLicenseLabel->setText(metadata.requiresLicense ? 
        (pluginInstance->IsLicensed() ? tr("Yes (Licensed)") : tr("Yes (Not Licensed)")) : 
        tr("No"));
    
    // Get plugin load info to show path and auto-load status
    const auto& loadedPlugins = pluginMgr.GetLoadedPlugins();
    auto it = loadedPlugins.find(pluginId.toStdString());
    if (it != loadedPlugins.end()) {
        m_pluginPathLabel->setText(QString::fromStdString(it->second.path.string()));
        m_autoLoadPluginCheck->setChecked(it->second.autoLoad);
        m_autoLoadPluginCheck->setEnabled(true);
    } else {
        m_pluginPathLabel->setText(tr("N/A"));
        m_autoLoadPluginCheck->setChecked(false);
        m_autoLoadPluginCheck->setEnabled(false);
    }
    
    m_enablePluginButton->setEnabled(true);
    m_unloadPluginButton->setEnabled(true);
    m_setLicenseButton->setEnabled(metadata.requiresLicense);
}

void StructuredConfigDialog::OnPluginSelectionChanged()
{
    UpdatePluginDetails();
}

void StructuredConfigDialog::OnBrowsePluginClicked()
{
    QString filter;
    filter = tr("Plugin Files (*.zip);;All Files (*)");

    QFileDialog dialog(this, tr("Select Plugin"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    
    dialog.setNameFilter(filter);
    dialog.setFileMode(QFileDialog::ExistingFile);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QString fileName = dialog.selectedFiles().value(0);
    
    if (!fileName.isEmpty())
    {
        m_pluginPathEdit->setText(fileName);
    }
}

void StructuredConfigDialog::OnLoadPluginClicked()
{
    QString path = m_pluginPathEdit->text();
    if (path.isEmpty())
    {
        QMessageBox::warning(this, tr("No Plugin Selected"),
            tr("Please select a plugin file to register."));
        return;
    }

    auto& pluginMgr = plugin::PluginManager::GetInstance();
    
    // Register plugin (copies to user directory and loads it)
    std::filesystem::path sourcePath(path.toStdString());
    auto result = pluginMgr.RegisterPlugin(sourcePath);
    
    if (result.isOk())
    {
        QMessageBox::information(this, tr("Plugin Registered"),
            tr("Plugin registered and enabled successfully: %1")
            .arg(QString::fromStdString(result.unwrap())));
        RefreshPluginsList();
        m_pluginPathEdit->clear();
    }
    else
    {
        QMessageBox::critical(this, tr("Registration Failed"),
            tr("Failed to register plugin:\n%1")
            .arg(QString::fromStdString(result.error().what())));
    }
}

void StructuredConfigDialog::OnUnloadPluginClicked()
{
    int row = m_pluginsTable->currentRow();
    if (row < 0) return;

    QString pluginId = m_pluginsTable->item(row, 1)->text();
    QString pluginName = m_pluginsTable->item(row, 0)->text();
    
    auto reply = QMessageBox::question(this, tr("Unregister Plugin"),
        tr("Are you sure you want to unregister plugin '%1'?\nThis will remove it permanently from your system.").arg(pluginName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes)
    {
        auto& pluginMgr = plugin::PluginManager::GetInstance();
        auto result = pluginMgr.UnregisterPlugin(pluginId.toStdString());
        
        if (result.isOk())
        {
            QMessageBox::information(this, tr("Plugin Unregistered"),
                tr("Plugin unregistered successfully."));
            RefreshPluginsList();
        }
        else
        {
            QMessageBox::critical(this, tr("Unregister Failed"),
                tr("Failed to unregister plugin:\n%1")
                .arg(QString::fromStdString(result.error().what())));
        }
    }
}

void StructuredConfigDialog::OnEnablePluginClicked()
{
    int row = m_pluginsTable->currentRow();
    if (row < 0) return;

    QString pluginId = m_pluginsTable->item(row, 1)->text();
    auto& pluginMgr = plugin::PluginManager::GetInstance();
    
    const auto& plugins = pluginMgr.GetLoadedPlugins();
    auto it = plugins.find(pluginId.toStdString());
    if (it == plugins.end()) return;
    
    bool currentlyEnabled = it->second.enabled;
    
    if (currentlyEnabled)
    {
        pluginMgr.DisablePlugin(pluginId.toStdString());
    }
    else
    {
        pluginMgr.EnablePlugin(pluginId.toStdString());
    }
    
    RefreshPluginsList();
    UpdatePluginDetails();
}

void StructuredConfigDialog::OnAutoLoadPluginToggled(bool checked)
{
    int row = m_pluginsTable->currentRow();
    if (row < 0) return;

    QString pluginId = m_pluginsTable->item(row, 1)->text();
    auto& pluginMgr = plugin::PluginManager::GetInstance();
    
    pluginMgr.SetPluginAutoLoad(pluginId.toStdString(), checked);
    
    util::Logger::Info("[StructuredConfigDialog] Plugin {} auto-load set to: {}",
        pluginId.toStdString(), checked);
}

void StructuredConfigDialog::OnSetLicenseClicked()
{
    int row = m_pluginsTable->currentRow();
    if (row < 0) return;

    QString licenseKey = m_pluginLicenseEdit->text();
    if (licenseKey.isEmpty())
    {
        QMessageBox::warning(this, tr("No License Key"),
            tr("Please enter a license key."));
        return;
    }

    QString pluginId = m_pluginsTable->item(row, 1)->text();
    auto& pluginMgr = plugin::PluginManager::GetInstance();
    auto* pluginInstance = pluginMgr.GetPlugin(pluginId.toStdString());
    
    if (!pluginInstance) return;
    
    if (pluginInstance->SetLicense(licenseKey.toStdString()))
    {
        QMessageBox::information(this, tr("License Set"),
            tr("License key accepted successfully."));
        m_pluginLicenseEdit->clear();
        UpdatePluginDetails();
    }
    else
    {
        QMessageBox::critical(this, tr("Invalid License"),
            tr("License key was rejected:\n%1")
            .arg(QString::fromStdString(pluginInstance->GetLastError())));
    }
}

void StructuredConfigDialog::OnRefreshPluginsClicked()
{
    auto& pluginMgr = plugin::PluginManager::GetInstance();
    auto discoveredPlugins = pluginMgr.DiscoverPlugins();
    
    int loadedCount = 0;
    for (const auto& pluginPath : discoveredPlugins)
    {
        auto result = pluginMgr.LoadPlugin(pluginPath);
        if (result.isOk())
        {
            loadedCount++;
        }
        else
        {
            util::Logger::Error("Failed to load plugin {}: {}", 
                pluginPath.string(), result.error().what());
        }
    }
    
    QMessageBox::information(this, tr("Plugins Loaded"),
        tr("Discovered %1 plugin(s), successfully loaded %2.")
        .arg(discoveredPlugins.size()).arg(loadedCount));
    
    RefreshPluginsList();
}

} // namespace ui::qt
