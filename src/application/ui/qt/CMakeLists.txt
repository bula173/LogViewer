message(STATUS "Building LogViewer::application Qt UI")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

target_sources(application_core
    PRIVATE
        ../MainWindowPresenter.cpp
        MainWindow.cpp
        StartupSplash.cpp
        panels/CanStatisticsStrategy.cpp
        panels/CanSignalTreePanel.cpp
        panels/LayoutManager.cpp
        panels/ActorDefinitionsPanel.cpp
        panels/ActorsPanel.cpp
        panels/BookmarksPanel.cpp
        panels/FilterProfilesPanel.cpp
        panels/FiltersPanel.cpp
        panels/ItemDetailsView.cpp
        panels/PatternAnalysisPanel.cpp
        panels/ScenariosPanel.cpp
        panels/SearchBar.cpp
        panels/SideBySidePanel.cpp
        panels/SearchResultsView.cpp
        panels/StatsSummaryPanel.cpp
        panels/TimeRangeFilterPanel.cpp
        panels/SignalPlotPanel.cpp
        panels/TimelineChartPanel.cpp
        panels/TraceViewerPanel.cpp
        panels/SequenceDiagramPanel.cpp
        dialogs/ConfigEditorDialog.cpp
        dialogs/FilterEditorDialog.cpp
        dialogs/GemmaDownloadDialog.cpp
        dialogs/LogFileLoadDialog.cpp
        dialogs/PluginManagerDialog.cpp
        dialogs/PreferencesDialog.cpp
        dialogs/StructuredConfigDialog.cpp
        dialogs/UpdateDialog.cpp
        events/EventsTableModel.cpp
        events/EventsTableView.cpp
        utils/ExportManager.cpp
        utils/FileTailer.cpp
        utils/SearchEngine.cpp
        utils/TypeFilterView.cpp
        utils/UpdateChecker.cpp
        widgets/FilterStatusBar.cpp
        widgets/TabBadgeManager.cpp
        widgets/PanelNavigator.cpp
)

# Expose this directory so that files in subdirectories can use
# cross-subdir includes like "panels/FiltersPanel.hpp", "events/EventsTableView.hpp", etc.
target_include_directories(application_core
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)
