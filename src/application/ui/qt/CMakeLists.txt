message(STATUS "Building LogViewer::application Qt UI")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

# qt_standard_project_setup()

target_sources(application_core
    PRIVATE
        ../MainWindowPresenter.cpp
        EventsTableModel.cpp
        EventsTableView.cpp
        FilterEditorDialog.cpp
        FiltersPanel.cpp
        ItemDetailsView.cpp
        MainWindow.cpp
        SearchResultsView.cpp
        TypeFilterView.cpp
        ConfigEditorDialog.cpp
        StructuredConfigDialog.cpp
        LogFileLoadDialog.cpp
        OllamaSetupDialog.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ConfigEditorDialog.hpp
        ${CMAKE_CURRENT_LIST_DIR}/EventsTableModel.hpp
        ${CMAKE_CURRENT_LIST_DIR}/EventsTableView.hpp
        ${CMAKE_CURRENT_LIST_DIR}/FilterEditorDialog.hpp
        ${CMAKE_CURRENT_LIST_DIR}/FiltersPanel.hpp
        ${CMAKE_CURRENT_LIST_DIR}/ItemDetailsView.hpp
        ${CMAKE_CURRENT_LIST_DIR}/LogFileLoadDialog.hpp
        ${CMAKE_CURRENT_LIST_DIR}/MainWindow.hpp
        ${CMAKE_CURRENT_LIST_DIR}/OllamaSetupDialog.hpp
        ${CMAKE_CURRENT_LIST_DIR}/SearchResultsView.hpp
        ${CMAKE_CURRENT_LIST_DIR}/StructuredConfigDialog.hpp
        ${CMAKE_CURRENT_LIST_DIR}/TypeFilterView.hpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ConfigEditorDialog.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/EventsTableModel.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/EventsTableView.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/FilterEditorDialog.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/FiltersPanel.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ItemDetailsView.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/LogFileLoadDialog.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/MainWindow.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/OllamaSetupDialog.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/SearchResultsView.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/StructuredConfigDialog.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/TypeFilterView.hpp>
        $<INSTALL_INTERFACE:ConfigEditorDialog.hpp>
        $<INSTALL_INTERFACE:EventsTableModel.hpp>
        $<INSTALL_INTERFACE:EventsTableView.hpp>
        $<INSTALL_INTERFACE:FilterEditorDialog.hpp>
        $<INSTALL_INTERFACE:FiltersPanel.hpp>
        $<INSTALL_INTERFACE:ItemDetailsView.hpp>
        $<INSTALL_INTERFACE:LogFileLoadDialog.hpp>
        $<INSTALL_INTERFACE:MainWindow.hpp>
        $<INSTALL_INTERFACE:OllamaSetupDialog.hpp>
        $<INSTALL_INTERFACE:SearchResultsView.hpp>
        $<INSTALL_INTERFACE:StructuredConfigDialog.hpp>
        $<INSTALL_INTERFACE:TypeFilterView.hpp>
)

