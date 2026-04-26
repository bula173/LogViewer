message(STATUS "Building LogViewer::application Qt UI")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

# qt_standard_project_setup()

target_sources(application_core
    PRIVATE
        ../MainWindowPresenter.cpp
        ActorDefinitionsPanel.cpp
        ActorsPanel.cpp
        EventsTableModel.cpp
        EventsTableView.cpp
        ExportManager.cpp
        StatsSummaryPanel.cpp
        UpdateChecker.cpp
        UpdateDialog.cpp
        FilterEditorDialog.cpp
        FiltersPanel.cpp
        ItemDetailsView.cpp
        MainWindow.cpp
        SearchResultsView.cpp
        TypeFilterView.cpp
        ConfigEditorDialog.cpp
        StructuredConfigDialog.cpp
        LogFileLoadDialog.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ConfigEditorDialog.hpp
        ${CMAKE_CURRENT_LIST_DIR}/ActorDefinition.hpp
        ${CMAKE_CURRENT_LIST_DIR}/ActorDefinitionsPanel.hpp
        ${CMAKE_CURRENT_LIST_DIR}/ActorsPanel.hpp
        ${CMAKE_CURRENT_LIST_DIR}/EventsTableModel.hpp
        ${CMAKE_CURRENT_LIST_DIR}/EventsTableView.hpp
        ${CMAKE_CURRENT_LIST_DIR}/ExportManager.hpp
        ${CMAKE_CURRENT_LIST_DIR}/StatsSummaryPanel.hpp
        ${CMAKE_CURRENT_LIST_DIR}/UpdateChecker.hpp
        ${CMAKE_CURRENT_LIST_DIR}/UpdateDialog.hpp
        ${CMAKE_CURRENT_LIST_DIR}/FilterEditorDialog.hpp
        ${CMAKE_CURRENT_LIST_DIR}/FiltersPanel.hpp
        ${CMAKE_CURRENT_LIST_DIR}/ItemDetailsView.hpp
        ${CMAKE_CURRENT_LIST_DIR}/LogFileLoadDialog.hpp
        ${CMAKE_CURRENT_LIST_DIR}/MainWindow.hpp
        ${CMAKE_CURRENT_LIST_DIR}/SearchResultsView.hpp
        ${CMAKE_CURRENT_LIST_DIR}/StructuredConfigDialog.hpp
        ${CMAKE_CURRENT_LIST_DIR}/TypeFilterView.hpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ConfigEditorDialog.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ActorDefinition.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ActorDefinitionsPanel.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ActorsPanel.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/EventsTableModel.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/EventsTableView.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ExportManager.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/StatsSummaryPanel.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/UpdateChecker.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/UpdateDialog.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/FilterEditorDialog.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/FiltersPanel.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ItemDetailsView.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/LogFileLoadDialog.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/MainWindow.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/SearchResultsView.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/StructuredConfigDialog.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/TypeFilterView.hpp>
        $<INSTALL_INTERFACE:ConfigEditorDialog.hpp>
        $<INSTALL_INTERFACE:ActorDefinition.hpp>
        $<INSTALL_INTERFACE:ActorDefinitionsPanel.hpp>
        $<INSTALL_INTERFACE:ActorsPanel.hpp>
        $<INSTALL_INTERFACE:EventsTableModel.hpp>
        $<INSTALL_INTERFACE:EventsTableView.hpp>
        $<INSTALL_INTERFACE:ExportManager.hpp>
        $<INSTALL_INTERFACE:StatsSummaryPanel.hpp>
        $<INSTALL_INTERFACE:UpdateChecker.hpp>
        $<INSTALL_INTERFACE:UpdateDialog.hpp>
        $<INSTALL_INTERFACE:FilterEditorDialog.hpp>
        $<INSTALL_INTERFACE:FiltersPanel.hpp>
        $<INSTALL_INTERFACE:ItemDetailsView.hpp>
        $<INSTALL_INTERFACE:LogFileLoadDialog.hpp>
        $<INSTALL_INTERFACE:MainWindow.hpp>
        $<INSTALL_INTERFACE:SearchResultsView.hpp>
        $<INSTALL_INTERFACE:StructuredConfigDialog.hpp>
        $<INSTALL_INTERFACE:TypeFilterView.hpp>
)

