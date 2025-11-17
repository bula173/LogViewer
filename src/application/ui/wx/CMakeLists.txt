message(STATUS "Building LogViewer::application wx UI")

find_package(wxWidgets REQUIRED COMPONENTS core base xml net adv html)
if(NOT wxWidgets_FOUND)
    message(FATAL_ERROR "wxWidgets not found. Please install wxWidgets or set the WXWIDGETS_ROOT environment variable.")
endif()

set(APP_WX_SRCS
    ../MainWindowPresenter.cpp
    ConfigEditorDialog.cpp
    CustomDataModel.cpp
    EventsContainerAdapter.cpp
    EventsVirtualListControl.cpp
    FilterEditorDialog.cpp
    FiltersPanel.cpp
    ItemVirtualListControl.cpp
    MainWindow.cpp
    SearchResultsView.cpp
    TypeFilterView.cpp
    WrappingTextRenderer.cpp
    WxErrorPresenter.cpp
)

add_library(application_wx STATIC ${APP_WX_SRCS})

include(${wxWidgets_USE_FILE})
target_compile_definitions(application_wx PUBLIC ${wxWidgets_DEFINITIONS})

target_link_libraries(application_wx
    PUBLIC
        application_core
        ${wxWidgets_LIBRARIES})

# Allow wx-specific headers to be included as "ui/wx/..."
target_include_directories(application_wx
    PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR})

# Suppress warnings from wx headers
target_include_directories(application_wx SYSTEM PUBLIC
    ${wxWidgets_INCLUDE_DIRS})

target_precompile_headers(application_wx
    PUBLIC
        <wx/wx.h>
        <wx/xml/xml.h>)
