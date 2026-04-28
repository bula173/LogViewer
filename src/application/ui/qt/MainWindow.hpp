#pragma once

#include "IMainWindowView.hpp"
#include "IUiPanels.hpp"
#include "ConfigObserver.hpp"
#include "UpdateInfo.hpp"
#include <memory>
#include "IPluginObserver.hpp"

#include <QMainWindow>
#include <QFutureWatcher>
#include <memory>
#include <vector>

class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QSplitter;
class QTabWidget;
class QWidget;
class QDragEnterEvent;
class QDropEvent;
class QDockWidget;

namespace mvc
{
class IController;
}

namespace db
{
class EventsContainer;
}

namespace ai
{
class IAIService;
class LogAnalyzer;
}

namespace plugin
{
class IPlugin;
}

namespace ui
{
class MainWindowPresenter;
}

namespace ui::qt
{

class SearchResultsView;
class TypeFilterView;
class ItemDetailsView;
class EventsTableView;
class FiltersPanel;
class StatsSummaryPanel;
class ActorsPanel;
class ActorDefinitionsPanel;
class SearchBar;
class UpdateChecker;

class MainWindow : public QMainWindow,
                   public ui::IMainWindowView,
                   public ui::ISearchResultsViewObserver,
                   public config::ConfigObserver,
                   public plugin::IPluginObserver
{
    Q_OBJECT

  public:
    MainWindow(mvc::IController& controller, db::EventsContainer& events,
        QWidget* parent = nullptr);
    ~MainWindow() override;

    // IMainWindowView implementation
    std::string ReadSearchQuery() const override;
    std::string CurrentStatusText() const override;
    void UpdateStatusText(const std::string& text) override;
    void SetSearchControlsEnabled(bool enabled) override;
    void ToggleProgressVisibility(bool visible) override;
    void ConfigureProgressRange(int range) override;
    void UpdateProgressValue(int value) override;
    void ProcessPendingEvents() override;
    void RefreshLayout() override;

    // ISearchResultsViewObserver
    void OnSearchResultActivated(long eventId) override;

    // IConfigObserver
    void OnConfigChanged() override;

    // IPluginObserver
    void OnPluginEvent(plugin::PluginEvent event, 
                      const std::string& pluginId,
                      plugin::IPlugin* plugin) override;

  private slots:
    void OnSearchRequested();
    void OnApplyFilterClicked();
    void OnExtendedFiltersChanged();
    void OnOpenFileRequested();
    void OnClearDataRequested();
    void OnOpenAppLogRequested();
    void OnExitRequested();
    void OnAboutRequested();
    void OnSetDarkTheme();
    void OnSetLightTheme();
    void OnSetSystemTheme();
    void OnRecentFileTriggered(const QString& filePath);
    void OnExportCsvRequested();
    void OnExportJsonRequested();
    void OnExportXmlRequested();
    void OnCheckForUpdates();
    void OnUpdateCheckComplete(updates::UpdateCheckResult result);
    void OnApplyPluginUpdate(QString pluginId, QString tempZipPath);

  private:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    void HandleDroppedFile(const QString& path);
    void InitializeUi(db::EventsContainer& events);
    void InitializePresenter(mvc::IController& controller,
        db::EventsContainer& events);
    void ApplyExtendedFilters();
    void SetupMenus();
    void RefreshRecentFilesMenu();
    void AddToRecentFiles(const QString& filePath);
    void LoadRecentFiles();
    void SaveRecentFiles();
    void ShowError(const QString& title, const QString& message);
    std::vector<int> GetRowsToExport() const;
    bool ShouldCheckForUpdates() const;
    void setupPluginManager();
    void loadPlugins();
    void reloadPlugins();
    void createPluginTab(const std::string& pluginId, plugin::IPlugin* plugin);
    void removePluginTab(const std::string& pluginId);
    // Left/side panel (previously "config" tab) for plugins. SDK-first plugins
    // may provide left-panel widgets via C-ABI and the host will insert them
    // into the left filter tabs or the plugin config dock as appropriate.
    void createPluginLeftTab(const std::string& pluginId, plugin::IPlugin* plugin);
    void removePluginLeftTab(const std::string& pluginId);

    // Helpers to simplify plugin panel embedding
    QWidget* CreateHostContainerForPluginWidget(QWidget* pluginWidget, QTabWidget* parentTabs);
    bool TryAddPluginMainPanel(const std::string& pluginId, plugin::IPlugin* plugin);
    bool TryAddPluginBottomPanel(const std::string& pluginId, plugin::IPlugin* plugin);
    bool TryAddPluginRightPanel(const std::string& pluginId, plugin::IPlugin* plugin);

    // Deprecated: legacy filter-tab helpers removed. Left/tab management is
    // handled by createPluginLeftTab/removePluginLeftTab.
    void RefreshPluginPanels();
    // Generic panel removal helpers (wrap legacy AI-specific names)
    void RemoveMainPanel();
    void RemoveLeftPanel();
    void RemoveBottomPanel();
    void RemoveRightPanel();

    QLineEdit* m_searchEdit {nullptr};
    QPushButton* m_searchButton {nullptr};
    QProgressBar* m_progressBar {nullptr};
    QLabel* m_statusLabel {nullptr};
    SearchResultsView* m_searchResults {nullptr};
    QPushButton* m_applyFilterButton {nullptr};
    QSplitter* m_bottomSplitter {nullptr};
    QSplitter* m_leftSplitter {nullptr};
    QSplitter* m_rightSplitter {nullptr};
    QTabWidget* m_filterTabs {nullptr};
    QTabWidget* m_contentTabs {nullptr};
    QTabWidget* m_bottomTabs {nullptr};
    EventsTableView* m_eventsView {nullptr};
    FiltersPanel* m_filtersPanel {nullptr};
    QWidget* m_bottomChatWidget {nullptr};

    // Dock widgets for collapsible panels
    QDockWidget* m_filtersDock {nullptr};
    QDockWidget* m_detailsDock {nullptr};
    QDockWidget* m_bottomDock {nullptr};
    QDockWidget* m_pluginLeftDock {nullptr};    // Generic plugin configuration dock (left-panel fallback)
    QTabWidget* m_pluginLeftTabs {nullptr};     // Tabs for multiple plugin configs / left-panel fallback

    std::unique_ptr<ui::MainWindowPresenter> m_presenter;
    TypeFilterView* m_typeFilterView {nullptr};
    ItemDetailsView* m_itemDetailsView {nullptr};
    QWidget* m_mainPanelWidget {nullptr};
    int m_mainPanelIndex {-1};
    db::EventsContainer* m_events {nullptr};
    
    // Recent files
    std::vector<QString> m_recentFiles;
    QMenu* m_recentFilesMenu {nullptr};
    static const int MAX_RECENT_FILES = 10;
    
    // Active plugin tracking
    std::string m_activePluginId;
    std::shared_ptr<ai::IAIService> m_pluginService;  // TODO: Generalize beyond AI-specific interface
    QWidget* m_bottomPluginPanel {nullptr};
    QTabWidget* m_rightTabs {nullptr};
    StatsSummaryPanel*      m_statsPanel   {nullptr};
    ActorsPanel*            m_actorsPanel  {nullptr};
    ActorDefinitionsPanel*  m_actorDefPanel{nullptr};
    SearchBar*              m_searchBar    {nullptr};
    UpdateChecker*          m_updateChecker{nullptr};
    QLabel*            m_updateBadge   {nullptr};
    updates::UpdateCheckResult m_lastUpdateResult;
    
    // Async filter state — prevents re-entrant filter runs while a worker is active
    QFutureWatcher<std::vector<unsigned long>>* m_filterWatcher {nullptr};
    bool m_filteringInProgress {false};

    // Plugin management
    std::map<std::string, int> m_pluginTabIndices;        // Maps plugin ID to content tab index
    std::map<std::string, int> m_pluginFilterTabIndices;  // Maps plugin ID to filter tab index
    std::map<std::string, int> m_pluginLeftTabIndices;    // Maps plugin ID to left/config tab index
    std::map<std::string, int> m_pluginRightTabIndices;   // Maps plugin ID to right dock tab index
};

} // namespace ui::qt
