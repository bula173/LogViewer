# Event Metrics Analyzer Plugin
# Provides frequency analysis and metrics for log events

set(PLUGIN_NAME event_metrics)
set(PLUGIN_ID "event_metrics_analyzer")
set(PLUGIN_VERSION "2.0.0")
set(PLUGIN_DISPLAY_NAME "Event Metrics")
set(PLUGIN_API_VERSION "1.0.0")

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)


# Create the plugin library
add_library(${PLUGIN_NAME} SHARED
    EventMetricsPlugin.cpp
    EventMetricsConfigWidget.cpp
    EventMetricsConfigWidget.hpp
)

# Enable Qt MOC for this plugin
set_target_properties(${PLUGIN_NAME} PROPERTIES
    AUTOMOC ON
    VERSION 1.0.0
    SOVERSION 1
    PREFIX ""
)

# Include directories
target_include_directories(${PLUGIN_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Link dependencies
target_link_libraries(${PLUGIN_NAME} PRIVATE
    application_core
    Qt6::Widgets
    Qt6::Core
)

# Installation
install(TARGETS ${PLUGIN_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/plugins
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins
)

set(PLUGIN_DEPENDENCIES_JSON "[]")

if(WIN32)
    set(PLUGIN_EXTENSION ".dll")
elseif(APPLE)
    set(PLUGIN_EXTENSION ".dylib")
else()
    set(PLUGIN_EXTENSION ".so")
endif()

set(PLUGIN_ENTRY "${PLUGIN_NAME}${PLUGIN_EXTENSION}")
set(PLUGIN_CONFIG_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/config.json.in")
set(PLUGIN_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/config.json")
# Set plugin output to build/plugins directory
set_target_properties(${PLUGIN_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

configure_file(${PLUGIN_CONFIG_TEMPLATE} ${PLUGIN_CONFIG} @ONLY)

# Create plugin package in build directory
set(PLUGIN_PACKAGE_DIR "${CMAKE_BINARY_DIR}/plugins/${PLUGIN_NAME}_package")
file(MAKE_DIRECTORY ${PLUGIN_PACKAGE_DIR})

add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PLUGIN_PACKAGE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PLUGIN_NAME}>
        ${PLUGIN_PACKAGE_DIR}/${PLUGIN_ENTRY}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PLUGIN_CONFIG}
        ${PLUGIN_PACKAGE_DIR}/config.json
    COMMAND ${CMAKE_COMMAND} -E tar "cf"
        "${CMAKE_BINARY_DIR}/plugins/${PLUGIN_NAME}.zip" --format=zip
        "${PLUGIN_ENTRY}" "config.json"
    WORKING_DIRECTORY ${PLUGIN_PACKAGE_DIR}
    COMMENT "Packaging ${PLUGIN_NAME}.zip"
)

message(STATUS "Building ${PLUGIN_NAME} plugin")
