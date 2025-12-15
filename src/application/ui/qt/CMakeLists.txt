message(STATUS "Building LogViewer::application Qt UI")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

qt_standard_project_setup()


set(APP_QT_SRCS
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
    OllamaSetupDialog.cpp)

add_library(application_qt STATIC ${APP_QT_SRCS})

target_link_libraries(application_qt
    PUBLIC
        application_core
        Qt6::Widgets
        Qt6::Concurrent)

target_include_directories(application_qt
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR})

# Set component for packaging
set_target_properties(application_qt PROPERTIES 
    CPACK_COMPONENT_NAME "qt_app"
)