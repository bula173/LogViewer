#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>

namespace ui::qt
{

/**
 * @brief In-app Plugin Manager dialog.
 *
 * Lists all plugins discovered in the plugins directory.  Each row shows the
 * plugin name, ID, version, type, and whether it is currently loaded.
 *
 * Actions:
 *   Install   — file picker → RegisterPlugin + LoadPlugin
 *   Uninstall — UnregisterPlugin + UnloadPlugin for the selected plugin
 *   Enable    — EnablePlugin (loads a registered-but-not-loaded plugin)
 *   Disable   — DisablePlugin (unloads without removing the file)
 *   Refresh   — rescan the plugins directory and reload the table
 */
class PluginManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PluginManagerDialog(QWidget* parent = nullptr);

private slots:
    void OnInstall();
    void OnUninstall();
    void OnEnable();
    void OnDisable();
    void OnRefresh();
    void OnSelectionChanged();

private:
    void BuildUi();
    void PopulateTable();
    QString TypeName(int type) const;

    QTableWidget* m_table      {nullptr};
    QPushButton*  m_installBtn {nullptr};
    QPushButton*  m_uninstallBtn{nullptr};
    QPushButton*  m_enableBtn  {nullptr};
    QPushButton*  m_disableBtn {nullptr};
    QLabel*       m_statusLabel{nullptr};
};

} // namespace ui::qt
