set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_library(application_core)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Charts)
find_package(LibArchive REQUIRED)
find_package(EXPAT REQUIRED)

# Link against required dependencies
target_link_libraries(application_core
    PRIVATE  # Changed internal libs to PRIVATE
        EXPAT::EXPAT
        LibArchive::LibArchive  # Assuming this is the correct target
        plugin_api
    PUBLIC   # Keep Qt public for plugins
        Qt6::Core
        Qt6::Widgets
        Qt6::Charts
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        fmt::fmt
)

# Set library properties
set_target_properties(application_core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Apply project-wide compiler warning flags (defined in cmake/Warnings.cmake)
logviewer_target_warnings(application_core)

# Submodules extend application_core via target_sources()
add_subdirectory(config)
add_subdirectory(db)
add_subdirectory(error)
add_subdirectory(filters)
add_subdirectory(mvc)
add_subdirectory(parsers)
add_subdirectory(plugins)
add_subdirectory(ui)
add_subdirectory(util)
add_subdirectory(version)

# Public include interface
target_include_directories(application_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/config>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/db>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/error>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/filters>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/mvc>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/parsers>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/plugins>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/plugin_api>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ui>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/util>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/version>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/main>
        $<INSTALL_INTERFACE:include>
)
