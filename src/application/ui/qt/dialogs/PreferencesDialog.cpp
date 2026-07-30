#include "PreferencesDialog.hpp"

#include "Config.hpp"
#include "Logger.hpp"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>
#include <QLabel>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QMessageBox>
#include <QDateTime>

namespace ui::qt
{

// Forward declarations for preference panels
class GeneralPreferencesPanel : public QWidget
{
  public:
    explicit GeneralPreferencesPanel(config::Config& config, QWidget* parent = nullptr)
        : QWidget(parent), m_config(config)
    {
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel("General Settings"));
        layout->addWidget(new QLabel("App Name: " + QString::fromStdString(config.GetAppName())));
        layout->addWidget(new QLabel("Config Path: " + QString::fromStdString(config.GetConfigFilePath())));
        layout->addStretch();
    }

    void apply() { /* Save settings */ }
    void reset() { /* Reset to last saved */ }

  private:
    config::Config& m_config;
};

class DisplayPreferencesPanel : public QWidget
{
  public:
    explicit DisplayPreferencesPanel(config::Config& config, QWidget* parent = nullptr)
        : QWidget(parent), m_config(config)
    {
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel("Display Settings"));
        layout->addWidget(new QLabel("Column Configuration"));
        layout->addWidget(new QLabel("Color Scheme"));
        layout->addStretch();
    }

    void apply() { /* Save settings */ }
    void reset() { /* Reset to last saved */ }

  private:
    config::Config& m_config;
};

class AIPreferencesPanel : public QWidget
{
  public:
    explicit AIPreferencesPanel(config::Config& config, QWidget* parent = nullptr)
        : QWidget(parent), m_config(config)
    {
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel("AI Settings"));
        layout->addWidget(new QLabel("Gemma Model Configuration"));
        layout->addWidget(new QLabel("API Provider Selection"));
        layout->addWidget(new QLabel("Inference Parameters"));
        layout->addStretch();
    }

    void apply() { /* Save settings */ }
    void reset() { /* Reset to last saved */ }

  private:
    config::Config& m_config;
};

class PluginsPreferencesPanel : public QWidget
{
  public:
    explicit PluginsPreferencesPanel(config::Config& config, QWidget* parent = nullptr)
        : QWidget(parent), m_config(config)
    {
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel("Plugins Settings"));
        layout->addWidget(new QLabel("Installed Plugins"));
        layout->addWidget(new QLabel("Plugin Configuration"));
        layout->addStretch();
    }

    void apply() { /* Save settings */ }
    void reset() { /* Reset to last saved */ }

  private:
    config::Config& m_config;
};

class PerformancePreferencesPanel : public QWidget
{
  public:
    explicit PerformancePreferencesPanel(config::Config& config, QWidget* parent = nullptr)
        : QWidget(parent), m_config(config)
    {
        auto* layout = new QVBoxLayout(this);
        layout->addWidget(new QLabel("Performance Settings"));
        layout->addWidget(new QLabel("Memory Management"));
        layout->addWidget(new QLabel("Threading Configuration"));
        layout->addWidget(new QLabel("Caching Options"));
        layout->addStretch();
    }

    void apply() { /* Save settings */ }
    void reset() { /* Reset to last saved */ }

  private:
    config::Config& m_config;
};

PreferencesDialog::PreferencesDialog(config::Config& config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
    , m_generalPanel(std::make_unique<GeneralPreferencesPanel>(config))
    , m_displayPanel(std::make_unique<DisplayPreferencesPanel>(config))
    , m_aiPanel(std::make_unique<AIPreferencesPanel>(config))
    , m_pluginsPanel(std::make_unique<PluginsPreferencesPanel>(config))
    , m_performancePanel(std::make_unique<PerformancePreferencesPanel>(config))
{
    setWindowTitle("Preferences");
    setModal(true);
    setMinimumWidth(700);
    setMinimumHeight(500);

    setupUI();
    loadCurrentSettings();
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::setupUI()
{
    auto* layout = new QVBoxLayout(this);

    // Create tab widget
    createTabs();
    layout->addWidget(m_tabWidget);

    // Create button bar
    createButtonBar();
    layout->addLayout(new QHBoxLayout());  // Placeholder for buttons
}

void PreferencesDialog::createTabs()
{
    m_tabWidget = new QTabWidget(this);

    // Add tabs in order of importance
    m_tabWidget->addTab(m_generalPanel.get(), "General");
    m_tabWidget->addTab(m_displayPanel.get(), "Display");
    m_tabWidget->addTab(m_aiPanel.get(), "AI");
    m_tabWidget->addTab(m_pluginsPanel.get(), "Plugins");
    m_tabWidget->addTab(m_performancePanel.get(), "Performance");
}

void PreferencesDialog::createButtonBar()
{
    auto* buttonLayout = new QHBoxLayout();

    // Left side: Import/Export
    m_importButton = new QPushButton("Import Settings...", this);
    m_exportButton = new QPushButton("Export Settings...", this);
    m_resetButton = new QPushButton("Reset to Defaults", this);

    connect(m_importButton, &QPushButton::clicked, this, &PreferencesDialog::onImportSettings);
    connect(m_exportButton, &QPushButton::clicked, this, &PreferencesDialog::onExportSettings);
    connect(m_resetButton, &QPushButton::clicked, this, &PreferencesDialog::onReset);

    buttonLayout->addWidget(m_importButton);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addStretch();

    // Right side: Standard buttons
    m_okButton = new QPushButton("OK", this);
    m_applyButton = new QPushButton("Apply", this);
    m_cancelButton = new QPushButton("Cancel", this);

    connect(m_okButton, &QPushButton::clicked, this, &PreferencesDialog::onOkay);
    connect(m_applyButton, &QPushButton::clicked, this, &PreferencesDialog::onApply);
    connect(m_cancelButton, &QPushButton::clicked, this, &PreferencesDialog::onCancel);

    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_cancelButton);

    // Add to main layout
    layout()->addItem(buttonLayout);
}

void PreferencesDialog::loadCurrentSettings()
{
    // Load current settings into all panels
    // This is called when dialog opens
    util::Logger::Debug("[PreferencesDialog] Loading current settings");
}

int PreferencesDialog::exec()
{
    loadCurrentSettings();
    return QDialog::exec();
}

void PreferencesDialog::onApply()
{
    util::Logger::Info("[PreferencesDialog] Applying settings");

    // Apply settings from all panels
    m_generalPanel->apply();
    m_displayPanel->apply();
    m_aiPanel->apply();
    m_pluginsPanel->apply();
    m_performancePanel->apply();

    // Save configuration to file
    m_config.SaveConfig();

    QMessageBox::information(this, "Settings Applied", "Your preferences have been saved successfully.");
}

void PreferencesDialog::onCancel()
{
    util::Logger::Debug("[PreferencesDialog] Cancelled - discarding changes");
    reject();
}

void PreferencesDialog::onReset()
{
    int ret = QMessageBox::question(this, "Reset to Defaults",
        "Are you sure you want to reset all settings to defaults? This cannot be undone.",
        QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        util::Logger::Info("[PreferencesDialog] Resetting to defaults");
        m_generalPanel->reset();
        m_displayPanel->reset();
        m_aiPanel->reset();
        m_pluginsPanel->reset();
        m_performancePanel->reset();
    }
}

void PreferencesDialog::onExportSettings()
{
    QString filename = QFileDialog::getSaveFileName(this,
        "Export Settings", "",
        "Settings Files (*.prefs.json);;All Files (*)");

    if (filename.isEmpty())
        return;

    util::Logger::Info("[PreferencesDialog] Exporting settings to {}", filename.toStdString());

    // Create JSON export of current settings
    QJsonObject root;
    root["version"] = "1.0";
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonDocument doc(root);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Export Failed", "Could not open file for writing: " + filename);
        return;
    }

    file.write(doc.toJson());
    file.close();

    QMessageBox::information(this, "Export Successful", "Settings exported to " + filename);
}

void PreferencesDialog::onImportSettings()
{
    QString filename = QFileDialog::getOpenFileName(this,
        "Import Settings", "",
        "Settings Files (*.prefs.json);;All Files (*)");

    if (filename.isEmpty())
        return;

    util::Logger::Info("[PreferencesDialog] Importing settings from {}", filename.toStdString());

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Import Failed", "Could not open file for reading: " + filename);
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        QMessageBox::warning(this, "Import Failed", "Invalid settings file format");
        return;
    }

    // Load settings from JSON
    loadCurrentSettings();

    QMessageBox::information(this, "Import Successful", "Settings imported from " + filename);
}

void PreferencesDialog::onOkay()
{
    onApply();
    accept();
}

}  // namespace ui::qt
