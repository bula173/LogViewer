#pragma once

#include "UpdateChecker.hpp"
#include "UpdateInfo.hpp"

#include <QDialog>
#include <QString>

class QLabel;
class QTextBrowser;
class QPushButton;
class QTableWidget;
class QProgressBar;

namespace ui::qt {

/**
 * @brief Modal dialog presenting available application and plugin updates.
 *
 * Shows:
 *  - App section (hidden when no newer app version): version badge,
 *    release notes, "Open Release Page" button.
 *  - Plugin table: Name | Installed | Available | API compat | checkbox.
 *    Incompatible rows have their checkbox disabled.
 *  - "Update Selected" button downloads checked plugins via UpdateChecker,
 *    then signals MainWindow to apply them.
 */
class UpdateDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit UpdateDialog(const updates::UpdateCheckResult& result,
                          UpdateChecker*                   checker,
                          QWidget*                         parent = nullptr);

  signals:
    /// Emitted when a plugin ZIP has been downloaded and is ready to install.
    void ApplyPluginUpdate(QString pluginId, QString tempZipPath);

  private slots:
    void OnUpdateSelectedClicked();
    void OnPluginProgress(QString pluginId, qint64 received, qint64 total);
    void OnPluginComplete(QString pluginId, QString tempPath);
    void OnPluginFailed(QString pluginId, QString error);

  private:
    void BuildLayout(const updates::UpdateCheckResult& result);
    void SetRowProgress(int row, int percent);

    UpdateChecker*                checker;
    updates::UpdateCheckResult    m_result;
    QTableWidget*                 m_pluginTable {nullptr};
    QPushButton*                  m_updateBtn   {nullptr};
    int                           m_pendingDownloads {0};
};

} // namespace ui::qt
