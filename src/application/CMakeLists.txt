message(STATUS "Building LogViewer::application")
message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

#-------------------------------------------------------------------------------
#Check if wxWidgets is available
find_package(wxWidgets REQUIRED COMPONENTS core base xml net adv html)
if(NOT wxWidgets_FOUND)
    message(FATAL_ERROR "wxWidgets not found. Please install wxWidgets or set the WXWIDGETS_ROOT environment variable.")
endif()

#--------------------------------------------------------------------------------

# Explicit source file list (avoids GLOB_RECURSE issues with build system)
set(APP_SRCS
    config/Config.cpp
    db/EventsContainer.cpp
    db/LogEvent.cpp
    filters/Filter.cpp
    filters/FilterManager.cpp
    gui/ConfigEditorDialog.cpp
    gui/EventsContainerAdapter.cpp
    gui/EventsVirtualListControl.cpp
    gui/FilterEditorDialog.cpp
    gui/FiltersPanel.cpp
    gui/ItemVirtualListControl.cpp
    gui/MainWindow.cpp
    xml/xmlParser.cpp
)

add_library(application STATIC ${APP_SRCS})

include(${wxWidgets_USE_FILE})
target_compile_definitions(application PUBLIC ${wxWidgets_DEFINITIONS})

target_link_libraries(application INTERFACE 
    ${wxWidgets_LIBRARIES} 
    nlohmann_json::nlohmann_json
    spdlog::spdlog_header_only
    expat)

target_include_directories(application PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR} 
    "${CMAKE_CURRENT_SOURCE_DIR}/../"
)

# Use SYSTEM to suppress warnings from third-party headers
target_include_directories(application SYSTEM PUBLIC
    ${wxWidgets_INCLUDE_DIRS} 
    ${nlohmann_json_SOURCE_DIR}/include
    ${spdlog_SOURCE_DIR}/include
    ${EXPAT_INCLUDE_DIRS}
)

# Precompiled headers for faster builds (30-50% improvement)
target_precompile_headers(application 
    PUBLIC
        <wx/wx.h>
        <wx/xml/xml.h>
        <nlohmann/json.hpp>
        <spdlog/spdlog.h>
)

#-------------------------------------------------------------------------------
