# Event Metrics Analyzer Plugin
# Provides frequency analysis and metrics for log events

set(PLUGIN_NAME event_metrics)

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

# Copy to dist/plugins for development
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    set(PLUGIN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/dist/plugins")
    file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})

    if(WIN32)
        set(PLUGIN_EXTENSION ".dll")
    elseif(APPLE)
        set(PLUGIN_EXTENSION ".dylib")
    else()
        set(PLUGIN_EXTENSION ".so")
    endif()

    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${PLUGIN_NAME}>
            ${PLUGIN_OUTPUT_DIR}/${PLUGIN_NAME}${PLUGIN_EXTENSION}
        COMMENT "Copying ${PLUGIN_NAME} to ${PLUGIN_OUTPUT_DIR}"
    )
endif()

message(STATUS "Building ${PLUGIN_NAME} plugin")
