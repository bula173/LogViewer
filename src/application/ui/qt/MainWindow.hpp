#pragma once

#include "IMainWindowView.hpp"
#include "IUiPanels.hpp"
#include "ConfigObserver.hpp"
#include "UpdateInfo.hpp"
#include "panels/LayoutManager.hpp"
#include "../services/IService.hpp"
#include <memory>
#include "IPluginObserver.hpp"

#include <QMainWindow>
#include <functional>
#include <memory>
#include <set>
#include <vector>

class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QWidget;
class QDragEnterEvent;
class QDropEvent;
class QDockWidget;

namespace services
{
class IService;
}

namespace mvc
{
class IController;
}

namespace parser
{
class IDataParser;
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

class StartupSplash;
class SearchResultsView;
class MainWindowFileOpsHelper;
class MainWindowFilterOpsHelper;
class MainWindowPluginOpsHelper;
class MainWindowExportOpsHelper;
class TypeFilterView;
class ItemDetailsView;
class EventsTableView;
class FiltersPanel;
class DashboardPanel;
class StatsSummaryPanel;
class PatternAnalysisPanel;
class SignalPlotPanel;
class TimelineChartPanel;
class TraceViewerPanel;
class SequenceDiagramPanel;
class BookmarksPanel;
class ScenariosPanel;
class ActorsPanel;
class ActorDefinitionsPanel;
class SearchBar;
class UnifiedSearchBar;
class UpdateChecker;
class TimeRangeFilterPanel;
class FilterProfilesPanel;
class CanSignalTreePanel;
class SideBySidePanel;
class FileTailer;
class FilterStatusBar;
class TabBadgeManager;
class PreferencesDialog;
class ShortcutsDialog;
struct FilterProfile;

class MainWindow : public QMainWindow,
                   public ui::IMainWindowView,
                   public ui::ISearchResultsViewObserver,
                   public config::ConfigObserver,
                   public plugin::IPluginObserver
{
    Q_OBJECT

  public:
    MainWindow(mvc::IController& controller, db::EventsContainer& events,
        StartupSplash* splash = nullptr, QWidget* parent = nullptr);
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
    std::string AskString(const std::string& title, const std::string& prompt,
        const std::string& defaultValue, bool& ok) override;
    void UpdateFilterStatus(int totalEvents, int filteredCount,
        int activeFilterCount, const std::string& filterDetails) override;

    // ISearchResultsViewObserver
    void OnSearchResultActivated(long eventId) override;

    // IConfigObserver
    void OnConfigChanged() override;

    // IPluginObserver
    void OnPluginEvent(plugin::PluginEvent event, 
                      const std::string& pluginId,
                      plugin::IPlugin* plugin) override;

    bool eventFilter(QObject* watched, QEvent* event) override;

    // Public UI refresh method (called by file operations helper)
    void RefreshRecentFilesMenu();

    // Public file loading method (called by file operations helper)
    void HandleDroppedFile(const QString& path);

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
    void OnGenerateReportFromDashboard();
    void OnCheckForUpdates();
    void OnUpdateCheckComplete(updates::UpdateCheckResult result);
    void OnApplyPluginUpdate(QString pluginId, QString tempZipPath);
    /// Gathers filter state from all panels and forwards it to FilterProfilesPanel::StoreProfile().
    void OnProfileSaveRequested(const QString& name);
    void OnProfileLoadRequested(const FilterProfile& profile);
    void OnSaveSession();
    void OnOpenSession();
    void OnLoadDbcRequested();
    void OnLoadEvlogTemplatesRequested();
    void OnSaveLayoutRequested();
    void OnDeleteLayoutRequested(const QString& name);
    void OnToggleTailRequested();
    void OnTailNewEvents(std::size_t count);
    void OnTailError(const QString& message);

  private:
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    /// Auto-switch to appropriate view based on file extension
    void AutoSwitchViewForFile(const QString& filePath);

    // Returns a parser appropriate for the file type, with DBC wired in for .asc files.
    std::unique_ptr<parser::IDataParser> CreateParserFor(const std::filesystem::path& path);
    void InitializeUi(db::EventsContainer& events);
    void InitializePresenter(mvc::IController& controller,
        db::EventsContainer& events);

    // Lazy panel refresh: mark all analysis panels dirty and schedule a
    // single debounced refresh of whichever tab is currently visible.
    void MarkAnalysisPanelsDirty();
    void RefreshCurrentAnalysisPanel();
    void ApplyExtendedFilters();
    void ApplyActorFilter();
    void ActivateSideBySide();
    void RunFilter(std::function<std::vector<unsigned long>()> worker,
                   const QString& statusMsg);
    void SetupMenus();
    void RefreshLayoutMenu();

    /// Apply a layout: restores dock state (user layouts) and tab visibility.
    void ApplyLayout(const LayoutDescriptor& layout);

    /// Capture the current window state as a named layout descriptor.
    [[nodiscard]] LayoutDescriptor CaptureLayout(const QString& name) const;
    void AddToRecentFiles(const QString& filePath);
    void LoadRecentFiles();
    void SaveRecentFiles();
    void ShowError(const QString& title, const QString& message);
    std::vector<int> GetRowsToExport() const;

    /// Return the last directory used for @p key, or @p fallback if unset.
    static QString LastDir(const QString& key, const QString& fallback);
    /// Persist the parent directory of @p filePath for @p key.
    static void SaveLastDir(const QString& key, const QString& filePath);
    /// Show a file-type picker when the extension is unknown; returns the chosen
    /// extension (e.g. ".evl") or an empty string if the user cancelled.
    QString PromptForFileType(const std::filesystem::path& path);
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
    void removePluginBottomTab(const std::string& pluginId);
    void removePluginRightTab(const std::string& pluginId);

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
    /// Remove all plugin-provided bottom-dock C-ABI panels (used by reloadPlugins()).
    void RemoveAllPluginBottomPanels();

    StartupSplash* m_splash {nullptr}; ///< non-owning; valid only during construction

    std::unique_ptr<LayoutManager> m_layoutManager;
    QMenu* m_layoutsMenu {nullptr};

    QLineEdit* m_searchEdit {nullptr};
    QPushButton* m_searchButton {nullptr};
    QProgressBar* m_progressBar {nullptr};
    QLabel* m_statusLabel {nullptr};
    SearchResultsView* m_searchResults {nullptr};
    QPushButton* m_applyFilterButton {nullptr};
    QSplitter* m_bottomSplitter {nullptr};
    QSplitter* m_leftSplitter {nullptr};
    QSplitter* m_rightSplitter {nullptr};
    QTabWidget*     m_filterTabs  {nullptr};
    QTabWidget*     m_contentTabs {nullptr};
    QStackedWidget* m_eventsStack {nullptr};
    QTabWidget*     m_bottomTabs  {nullptr};
    EventsTableView* m_eventsView {nullptr};
    FiltersPanel* m_filtersPanel {nullptr};
    QWidget* m_bottomChatWidget {nullptr};

    // Dock widgets for collapsible panels
    QDockWidget* m_filtersDock       {nullptr};
    QDockWidget* m_signalBrowserDock {nullptr}; // Signal Browser — tabbed alongside Filters
    QDockWidget* m_detailsDock       {nullptr};
    QDockWidget* m_bottomDock        {nullptr};
    QDockWidget* m_pluginLeftDock    {nullptr}; // Generic plugin configuration dock (left-panel fallback)
    QTabWidget* m_pluginLeftTabs {nullptr};     // Tabs for multiple plugin configs / left-panel fallback

    std::unique_ptr<ui::MainWindowPresenter> m_presenter;

    // Architecture helpers for separation of concerns
    std::unique_ptr<MainWindowFileOpsHelper> m_fileOpsHelper;
    std::unique_ptr<MainWindowFilterOpsHelper> m_filterOpsHelper;
    std::unique_ptr<MainWindowPluginOpsHelper> m_pluginOpsHelper;
    std::unique_ptr<MainWindowExportOpsHelper> m_exportOpsHelper;

    TypeFilterView* m_typeFilterView {nullptr};
    ItemDetailsView* m_itemDetailsView {nullptr};
    QWidget* m_mainPanelWidget {nullptr};
    int m_mainPanelIndex {-1};
    db::EventsContainer* m_events {nullptr};
    
    QString m_currentLogFilePath;
    QString m_currentDbcFilePath;          ///< Optional DBC for CAN signal decoding
    QString m_evlogTemplateDir;            ///< Optional template dir for evlog BINARY payloads

    // Recent files menu (state owned by m_fileOpsHelper)
    QMenu* m_recentFilesMenu {nullptr};

    // Active plugin tracking
    std::shared_ptr<services::IService> m_currentService;
    QTabWidget* m_rightTabs {nullptr};
    DashboardPanel*         m_dashboardPanel{nullptr};
    StatsSummaryPanel*      m_statsPanel    {nullptr};
    PatternAnalysisPanel*   m_patternPanel  {nullptr};
    SignalPlotPanel*        m_signalPlotPanel {nullptr};
    TimelineChartPanel*     m_timelinePanel   {nullptr};
    TraceViewerPanel*       m_tracePanel      {nullptr};
    SequenceDiagramPanel*   m_sequencePanel   {nullptr};
    BookmarksPanel*         m_bookmarksPanel{nullptr};
    ScenariosPanel*         m_scenariosPanel{nullptr};
    ActorsPanel*            m_actorsPanel   {nullptr};
    ActorDefinitionsPanel*  m_actorDefPanel{nullptr};
    SearchBar*              m_searchBar    {nullptr};
    UnifiedSearchBar*       m_unifiedSearchBar{nullptr};
    UpdateChecker*          m_updateChecker{nullptr};
    FileTailer*             m_tailer       {nullptr};
    QAction*                m_tailAction   {nullptr};
    TimeRangeFilterPanel*   m_timeRangePanel{nullptr};
    FilterProfilesPanel*    m_profilesPanel {nullptr};
    CanSignalTreePanel*     m_canSignalTree    {nullptr};
    SideBySidePanel*        m_sideBySidePanel  {nullptr};
    FilterStatusBar*        m_filterStatusBar  {nullptr};
    TabBadgeManager*        m_tabBadgeManager  {nullptr};
    QLabel*            m_updateBadge   {nullptr};
    updates::UpdateCheckResult m_lastUpdateResult;
    
    bool m_filteringInProgress {false};
    bool m_searchInProgress {false};

    // Lazy analysis-panel refresh — panels only recompute when visible
    QTimer*             m_panelRefreshTimer  {nullptr};
    std::set<QWidget*>  m_dirtyPanels;
    // Debounced in-panel search (old inline SearchBar) — avoids O(n×m) rebuild on every keystroke
    QTimer*             m_searchDebounceTimer {nullptr};
    QString             m_pendingSearchTerm;
    bool                m_pendingSearchCase  {false};
    // Debounced Tools-panel search (UnifiedSearchBar) — triggers OnSearchRequested()
    QTimer*             m_unifiedSearchDebounceTimer {nullptr};

    // Plugin management
    // Widgets (not indices) are stored so tabs remain trackable after the user
    // drags/reorders them — the live index is resolved via indexOf() on demand.
    std::map<std::string, QWidget*> m_pluginTabIndices;        // Maps plugin ID to content tab widget
    std::map<std::string, int> m_pluginFilterTabIndices;       // Maps plugin ID to filter tab index (unused)
    std::map<std::string, QWidget*> m_pluginLeftTabIndices;    // Maps plugin ID to left/config tab widget
    std::map<std::string, QWidget*> m_pluginRightTabIndices;   // Maps plugin ID to right dock tab widget
    std::map<std::string, QWidget*> m_pluginBottomTabIndices;  // Maps plugin ID to bottom dock tab widget

    /// Show a right-click context menu on a tab bar with sort/reorder actions.
    void ShowTabContextMenu(QTabWidget* tabWidget, const QPoint& pos);
};

} // namespace ui::qt
