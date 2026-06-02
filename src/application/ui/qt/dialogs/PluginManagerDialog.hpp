#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>

namespace ui::qt
{

/**
 * @brief In-app Plugin Manager dialog (Tools > Manage Plugins…).
 *
 * Lists all plugins discovered in the plugins directory.  Each row shows the
 * plugin name, ID, version, type, and runtime status.  Selecting a row
 * populates the details pane below with author, description, path, license
 * status, and auto-load preference.
 *
 * Actions:
 *   Install     — file picker → RegisterPlugin (extracts ZIP and loads)
 *   Uninstall   — UnregisterPlugin for the selected plugin
 *   Enable      — EnablePlugin (loads a registered-but-not-running plugin)
 *   Disable     — DisablePlugin (unloads without removing the file)
 *   Auto-load   — toggles SetPluginAutoLoad per-plugin
 *   Set License — applies a license key to the selected plugin
 *   Refresh     — rescan the plugins directory and reload the table
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
    void OnAutoLoadToggled(bool checked);
    void OnSetLicense();
    void OnRefresh();
    void OnSelectionChanged();

private:
    void BuildUi();
    void PopulateTable();
    void UpdateDetails();
    void ClearDetails();
    QString StatusText(int status) const;
    QString TypeName(int type) const;

    // ── Table ──────────────────────────────────────────────────────────────
    QTableWidget* m_table       {nullptr};

    // ── Action buttons ─────────────────────────────────────────────────────
    QPushButton*  m_installBtn  {nullptr};
    QPushButton*  m_uninstallBtn{nullptr};
    QPushButton*  m_enableBtn   {nullptr};
    QPushButton*  m_disableBtn  {nullptr};
    QPushButton*  m_refreshBtn  {nullptr};

    // ── Details pane ───────────────────────────────────────────────────────
    QLabel*      m_detailName    {nullptr};
    QLabel*      m_detailId      {nullptr};
    QLabel*      m_detailVersion {nullptr};
    QLabel*      m_detailAuthor  {nullptr};
    QLabel*      m_detailType    {nullptr};
    QLabel*      m_detailStatus  {nullptr};
    QLabel*      m_detailLicense {nullptr};
    QLabel*      m_detailPath    {nullptr};
    QLabel*      m_detailDesc    {nullptr};
    QCheckBox*   m_autoLoadCheck {nullptr};

    // License key row
    QLineEdit*   m_licenseEdit   {nullptr};
    QPushButton* m_licenseBtn    {nullptr};

    QLabel*      m_statusLabel   {nullptr};
};

} // namespace ui::qt
