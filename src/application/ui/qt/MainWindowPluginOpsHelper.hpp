#pragma once

#include <QString>
#include <vector>
#include <memory>

class QMainWindow;
class QWidget;
class QTabWidget;
class QDockWidget;

namespace plugin { class PluginManager; }
namespace ui::qt {

class MainWindow;

/**
 * @brief Helper class encapsulating plugin operations for MainWindow
 *
 * Delegates plugin management from MainWindow, reducing its complexity.
 * Manages:
 * - Plugin loading and initialization
 * - Plugin unloading and cleanup
 * - Plugin UI panel integration (main, left, right, bottom)
 * - Plugin lifecycle events
 *
 * Maintains reference to PluginManager for state management.
 */
class MainWindowPluginOpsHelper {
public:
    explicit MainWindowPluginOpsHelper(MainWindow* mainWindow);
    ~MainWindowPluginOpsHelper() = default;

    // Plugin lifecycle
    void LoadPlugins();
    void ReloadPlugins();
    void SetupPluginManager();

    // Plugin events
    void OnPluginEvent(const QString& pluginName, const QString& eventType);

    // Plugin UI panel management
    bool TryAddPluginBottomPanel(QWidget* widget, const QString& title);
    bool TryAddPluginMainPanel(QWidget* widget, const QString& title);
    bool TryAddPluginRightPanel(QWidget* widget, const QString& title);
    void RemovePluginLeftTab(const QString& tabName);
    void RemovePluginTab(const QString& tabName);
    void RefreshPluginPanels();

    // State accessors
    int GetLoadedPluginCount() const;
    bool IsPluginLoaded(const QString& name) const;

private:
    MainWindow* m_mainWindow;
    plugin::PluginManager* m_pluginManager;
    std::vector<QString> m_loadedPlugins;

    void InitializePluginPanels();
    void ConnectPluginSignals();
};

} // namespace ui::qt
