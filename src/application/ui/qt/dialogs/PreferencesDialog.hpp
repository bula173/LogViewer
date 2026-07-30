#pragma once

#include <QDialog>
#include <memory>

class QTabWidget;
class QPushButton;

namespace config
{
class Config;
}

namespace ui::qt
{

class GeneralPreferencesPanel;
class DisplayPreferencesPanel;
class AIPreferencesPanel;
class PluginsPreferencesPanel;
class PerformancePreferencesPanel;

/**
 * @brief Unified Preferences dialog consolidating all application settings.
 *
 * Organizes settings into logical categories:
 * - General (app name, paths, logging)
 * - Display (columns, colors, themes)
 * - AI (model selection, API keys, inference settings)
 * - Plugins (plugin management, configuration)
 * - Performance (caching, threading, buffer sizes)
 * - Advanced (debug options, expert settings)
 */
class PreferencesDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit PreferencesDialog(config::Config& config, QWidget* parent = nullptr);
    ~PreferencesDialog() override;

    /// Show dialog and return true if settings were applied
    int exec() override;

  private slots:
    void onApply();
    void onCancel();
    void onReset();
    void onExportSettings();
    void onImportSettings();
    void onOkay();

  private:
    void setupUI();
    void createTabs();
    void createButtonBar();
    void loadCurrentSettings();

    config::Config& m_config;

    // Tab panels
    QTabWidget* m_tabWidget {nullptr};
    std::unique_ptr<GeneralPreferencesPanel> m_generalPanel;
    std::unique_ptr<DisplayPreferencesPanel> m_displayPanel;
    std::unique_ptr<AIPreferencesPanel> m_aiPanel;
    std::unique_ptr<PluginsPreferencesPanel> m_pluginsPanel;
    std::unique_ptr<PerformancePreferencesPanel> m_performancePanel;

    // Buttons
    QPushButton* m_okButton {nullptr};
    QPushButton* m_applyButton {nullptr};
    QPushButton* m_cancelButton {nullptr};
    QPushButton* m_resetButton {nullptr};
    QPushButton* m_exportButton {nullptr};
    QPushButton* m_importButton {nullptr};
};

}  // namespace ui::qt
