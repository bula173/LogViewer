#pragma once

#include <QString>
#include <vector>
#include <memory>
#include <filesystem>

class QWidget;
class QMainWindow;
class QMenu;
class QComboBox;
class QFile;

namespace db { class EventsContainer; }
namespace mvc { class IController; }
namespace ui::qt {

class DashboardPanel;
class MainWindow;

/**
 * @brief Helper class encapsulating file operations for MainWindow
 *
 * Delegates file I/O operations from MainWindow, reducing its complexity.
 * Manages:
 * - File loading with auto-detection or explicit parser selection
 * - Session save/load
 * - Recent files list
 * - DBC/EVLOG template loading
 * - File drop handling
 *
 * Maintains state for file operations and reports back to MainWindow
 * via callbacks and signal-like patterns.
 *
 * Thread-safe: File operations happen on worker threads; main thread
 * is notified via Qt signals connected in MainWindow.
 */
class MainWindowFileOpsHelper {
public:
    explicit MainWindowFileOpsHelper(ui::qt::MainWindow* mainWindow);
    ~MainWindowFileOpsHelper() = default;

    // File operations - delegate these from MainWindow
    void OnOpenFileRequested();
    void OnLoadDbcRequested();
    void OnLoadEvlogTemplatesRequested();

    // Session management
    void OnSaveSession();
    void OnOpenSession();

    // Auto-view switching based on file type
    void AutoSwitchViewForFile(const QString& filePath);

    // Handle files dropped on window
    void HandleDroppedFile(const QString& path);

    // Recent files management
    void LoadRecentFiles();
    void SaveRecentFiles();
    void AddToRecentFiles(const QString& filePath);
    void RefreshRecentFilesMenu();
    void OnRecentFileTriggered(const QString& filePath);

    // Access recent files for UI
    const std::vector<QString>& GetRecentFiles() const { return m_recentFiles; }
    QString GetLastDirectory() const { return m_lastDirectory; }
    void SetLastDirectory(const QString& dir) { m_lastDirectory = dir; }

private:
    ui::qt::MainWindow* m_mainWindow;
    std::vector<QString> m_recentFiles;
    QString m_lastDirectory;
    static constexpr int MAX_RECENT_FILES = 10;

    // Internal file loading with progress tracking
    void LoadFileInternal(const std::filesystem::path& path);

    // Session file paths
    QString GetSessionFilePath() const;
    bool ValidateSessionFile(const QString& path) const;
};

} // namespace ui::qt
