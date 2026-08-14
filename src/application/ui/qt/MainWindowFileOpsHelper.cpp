#include "MainWindowFileOpsHelper.hpp"
#include "MainWindow.hpp"
#include "Logger.hpp"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <algorithm>

namespace ui::qt {

MainWindowFileOpsHelper::MainWindowFileOpsHelper(MainWindow* mainWindow)
    : m_mainWindow(mainWindow)
{
    if (!m_mainWindow) {
        util::Logger::Error("[FileOpsHelper] MainWindow pointer is null");
    }

    QSettings settings;
    m_lastDirectory = settings.value("lastDirectory",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
}

void MainWindowFileOpsHelper::LoadRecentFiles()
{
    if (!m_mainWindow) return;

    QSettings settings;
    settings.beginGroup("RecentFiles");
    int size = settings.beginReadArray("files");

    m_recentFiles.clear();
    for (int i = 0; i < size && i < MAX_RECENT_FILES; ++i) {
        settings.setArrayIndex(i);
        QString path = settings.value("path", "").toString();
        if (!path.isEmpty() && QFile::exists(path)) {
            m_recentFiles.push_back(path);
        }
    }
    settings.endArray();
    settings.endGroup();

    util::Logger::Debug("[FileOpsHelper] Loaded {} recent files", m_recentFiles.size());
}

void MainWindowFileOpsHelper::SaveRecentFiles()
{
    if (!m_mainWindow) return;

    QSettings settings;
    settings.beginGroup("RecentFiles");
    settings.beginWriteArray("files");

    for (size_t i = 0; i < m_recentFiles.size(); ++i) {
        settings.setArrayIndex(static_cast<int>(i));
        settings.setValue("path", m_recentFiles[i]);
    }
    settings.endArray();
    settings.endGroup();
    settings.sync();

    util::Logger::Debug("[FileOpsHelper] Saved {} recent files", m_recentFiles.size());
}

void MainWindowFileOpsHelper::AddToRecentFiles(const QString& filePath)
{
    if (filePath.isEmpty() || !m_mainWindow) return;

    auto it = std::ranges::find(m_recentFiles, filePath);
    if (it != m_recentFiles.end()) {
        m_recentFiles.erase(it);
    }

    m_recentFiles.insert(m_recentFiles.begin(), filePath);

    while (m_recentFiles.size() > static_cast<size_t>(MAX_RECENT_FILES)) {
        m_recentFiles.pop_back();
    }

    m_lastDirectory = QFileInfo(filePath).absolutePath();

    SaveRecentFiles();
    RefreshRecentFilesMenu();

    util::Logger::Debug("[FileOpsHelper] Added to recent: {}", filePath.toStdString());
}

void MainWindowFileOpsHelper::RefreshRecentFilesMenu()
{
    if (!m_mainWindow) return;

    // Call back to MainWindow's public method to refresh the menu UI
    m_mainWindow->RefreshRecentFilesMenu();
    util::Logger::Trace("[FileOpsHelper] Recent files menu refreshed");
}

void MainWindowFileOpsHelper::OnRecentFileTriggered(const QString& filePath)
{
    if (!m_mainWindow || filePath.isEmpty()) return;

    if (!QFile::exists(filePath)) {
        util::Logger::Warn("[FileOpsHelper] Recent file not found: {}", filePath.toStdString());
        return;
    }

    util::Logger::Debug("[FileOpsHelper] Opening recent file: {}", filePath.toStdString());

    // Delegate to MainWindow to actually load the file
    // This will trigger the file loading pipeline through the main controller
    m_mainWindow->HandleDroppedFile(filePath);
}

void MainWindowFileOpsHelper::OnOpenFileRequested()
{
    if (!m_mainWindow) return;

    QString filePath = QFileDialog::getOpenFileName(
        m_mainWindow,
        QObject::tr("Open Log File"),
        m_lastDirectory,
        QObject::tr("All Files (*);;XML Files (*.xml);;JSON Files (*.json);;CSV Files (*.csv)")
    );

    if (filePath.isEmpty()) return;

    m_lastDirectory = QFileInfo(filePath).absolutePath();
    QSettings settings;
    settings.setValue("lastDirectory", m_lastDirectory);

    AddToRecentFiles(filePath);

    util::Logger::Debug("[FileOpsHelper] File selected: {}", filePath.toStdString());
}

void MainWindowFileOpsHelper::OnLoadDbcRequested()
{
    if (!m_mainWindow) return;

    QString dbcPath = QFileDialog::getOpenFileName(
        m_mainWindow,
        QObject::tr("Load DBC File"),
        m_lastDirectory,
        QObject::tr("DBC Files (*.dbc);;All Files (*)")
    );

    if (dbcPath.isEmpty()) return;

    util::Logger::Debug("[FileOpsHelper] DBC file selected: {}", dbcPath.toStdString());
}

void MainWindowFileOpsHelper::OnLoadEvlogTemplatesRequested()
{
    if (!m_mainWindow) return;

    QString templatePath = QFileDialog::getOpenFileName(
        m_mainWindow,
        QObject::tr("Load EVLOG Templates"),
        m_lastDirectory,
        QObject::tr("JSON Files (*.json);;All Files (*)")
    );

    if (templatePath.isEmpty()) return;

    util::Logger::Debug("[FileOpsHelper] Template file selected: {}", templatePath.toStdString());
}

void MainWindowFileOpsHelper::OnSaveSession()
{
    if (!m_mainWindow) return;

    util::Logger::Debug("[FileOpsHelper] Save session requested");
}

void MainWindowFileOpsHelper::OnOpenSession()
{
    if (!m_mainWindow) return;

    QString sessionPath = QFileDialog::getOpenFileName(
        m_mainWindow,
        QObject::tr("Open Session"),
        m_lastDirectory,
        QObject::tr("Session Files (*.session);;All Files (*)")
    );

    if (sessionPath.isEmpty()) return;

    util::Logger::Debug("[FileOpsHelper] Session file selected: {}", sessionPath.toStdString());
}

void MainWindowFileOpsHelper::AutoSwitchViewForFile(const QString& filePath)
{
    if (!m_mainWindow || filePath.isEmpty()) return;

    QString ext = QFileInfo(filePath).suffix().toLower();
    util::Logger::Debug("[FileOpsHelper] Auto-switching view for file type: {}", ext.toStdString());
}

void MainWindowFileOpsHelper::HandleDroppedFile(const QString& path)
{
    if (!m_mainWindow || path.isEmpty()) return;

    if (QFileInfo(path).isFile()) {
        AddToRecentFiles(path);
        util::Logger::Debug("[FileOpsHelper] File dropped: {}", path.toStdString());
    }
}

QString MainWindowFileOpsHelper::GetSessionFilePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appDataPath + "/session.json";
}

bool MainWindowFileOpsHelper::ValidateSessionFile(const QString& path) const
{
    if (path.isEmpty() || !QFile::exists(path)) {
        return false;
    }

    return true;
}

} // namespace ui::qt
