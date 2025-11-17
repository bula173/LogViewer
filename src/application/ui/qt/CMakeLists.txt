message(STATUS "Building LogViewer::application Qt UI")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(APP_QT_SRCS
    ../MainWindowPresenter.cpp
    EventsTableModel.cpp
    EventsTableView.cpp
    FilterEditorDialog.cpp
    FiltersPanel.cpp
    ItemDetailsView.cpp
    MainWindow.cpp
    SearchResultsView.cpp
    TypeFilterView.cpp)

add_library(application_qt STATIC ${APP_QT_SRCS})

target_link_libraries(application_qt
    PUBLIC
        application_core
        Qt${QT_VERSION_MAJOR}::Widgets)

target_include_directories(application_qt
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR})
