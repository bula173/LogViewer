message(STATUS "Building LogViewer::application")
message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

#-------------------------------------------------------------------------------
#Check if wxWidgets is available
find_package(wxWidgets REQUIRED COMPONENTS core base xml net adv html)
if(NOT wxWidgets_FOUND)
    message(FATAL_ERROR "wxWidgets not found. Please install wxWidgets or set the WXWIDGETS_ROOT environment variable.")
endif()

#--------------------------------------------------------------------------------

# Enable exporting compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE APP_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

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

#-------------------------------------------------------------------------------
