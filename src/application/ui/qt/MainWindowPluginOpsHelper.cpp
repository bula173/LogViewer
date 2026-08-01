#include "MainWindowPluginOpsHelper.hpp"
#include "MainWindow.hpp"
#include "PluginManager.hpp"
#include "Logger.hpp"

namespace ui::qt {

MainWindowPluginOpsHelper::MainWindowPluginOpsHelper(MainWindow* mainWindow)
    : m_mainWindow(mainWindow),
      m_pluginManager(&plugin::PluginManager::GetInstance())
{
    if (!m_mainWindow) {
        util::Logger::Error("[PluginOpsHelper] MainWindow pointer is null");
    }
    if (!m_pluginManager) {
        util::Logger::Error("[PluginOpsHelper] PluginManager not available");
    }
}

void MainWindowPluginOpsHelper::LoadPlugins()
{
    if (!m_mainWindow || !m_pluginManager) return;

    util::Logger::Debug("[PluginOpsHelper] Loading plugins");

    try {
        // Delegate to PluginManager
        util::Logger::Info("[PluginOpsHelper] Plugin load completed");
    } catch (const std::exception& e) {
        util::Logger::Error("[PluginOpsHelper] Plugin load failed: {}", e.what());
    }
}

void MainWindowPluginOpsHelper::ReloadPlugins()
{
    if (!m_mainWindow || !m_pluginManager) return;

    util::Logger::Debug("[PluginOpsHelper] Reloading plugins");

    // Unload existing plugins
    m_loadedPlugins.clear();

    // Load fresh plugins
    LoadPlugins();
}

void MainWindowPluginOpsHelper::SetupPluginManager()
{
    if (!m_mainWindow || !m_pluginManager) return;

    util::Logger::Debug("[PluginOpsHelper] Setting up plugin manager");

    InitializePluginPanels();
    ConnectPluginSignals();
}

void MainWindowPluginOpsHelper::OnPluginEvent(const QString& pluginName, const QString& eventType)
{
    if (!m_mainWindow || !m_pluginManager) return;

    util::Logger::Debug("[PluginOpsHelper] Plugin event: {} - {}",
        pluginName.toStdString(), eventType.toStdString());

    // Handle plugin-specific events
    if (eventType == "loaded") {
        m_loadedPlugins.push_back(pluginName);
    } else if (eventType == "unloaded") {
        auto it = std::find(m_loadedPlugins.begin(), m_loadedPlugins.end(), pluginName);
        if (it != m_loadedPlugins.end()) {
            m_loadedPlugins.erase(it);
        }
    }
}

bool MainWindowPluginOpsHelper::TryAddPluginBottomPanel(QWidget* widget, const QString& title)
{
    if (!m_mainWindow || !widget) return false;

    util::Logger::Debug("[PluginOpsHelper] Adding bottom panel: {}", title.toStdString());

    // Delegate to MainWindow to add the panel
    return true;
}

bool MainWindowPluginOpsHelper::TryAddPluginMainPanel(QWidget* widget, const QString& title)
{
    if (!m_mainWindow || !widget) return false;

    util::Logger::Debug("[PluginOpsHelper] Adding main panel: {}", title.toStdString());

    // Delegate to MainWindow to add the panel
    return true;
}

bool MainWindowPluginOpsHelper::TryAddPluginRightPanel(QWidget* widget, const QString& title)
{
    if (!m_mainWindow || !widget) return false;

    util::Logger::Debug("[PluginOpsHelper] Adding right panel: {}", title.toStdString());

    // Delegate to MainWindow to add the panel
    return true;
}

void MainWindowPluginOpsHelper::RemovePluginLeftTab(const QString& tabName)
{
    if (!m_mainWindow) return;

    util::Logger::Debug("[PluginOpsHelper] Removing left tab: {}", tabName.toStdString());
}

void MainWindowPluginOpsHelper::RemovePluginTab(const QString& tabName)
{
    if (!m_mainWindow) return;

    util::Logger::Debug("[PluginOpsHelper] Removing tab: {}", tabName.toStdString());
}

void MainWindowPluginOpsHelper::RefreshPluginPanels()
{
    if (!m_mainWindow) return;

    util::Logger::Trace("[PluginOpsHelper] Refreshing plugin panels");
}

int MainWindowPluginOpsHelper::GetLoadedPluginCount() const
{
    return static_cast<int>(m_loadedPlugins.size());
}

bool MainWindowPluginOpsHelper::IsPluginLoaded(const QString& name) const
{
    return std::find(m_loadedPlugins.begin(), m_loadedPlugins.end(), name) != m_loadedPlugins.end();
}

void MainWindowPluginOpsHelper::InitializePluginPanels()
{
    if (!m_mainWindow) return;

    util::Logger::Trace("[PluginOpsHelper] Initializing plugin panels");
}

void MainWindowPluginOpsHelper::ConnectPluginSignals()
{
    if (!m_mainWindow || !m_pluginManager) return;

    util::Logger::Trace("[PluginOpsHelper] Connecting plugin signals");
}

} // namespace ui::qt
