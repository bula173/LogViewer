# Log Anomaly Metrics Plugin
# Analyzer plugin leveraging Eigen and Boost for metric computation

set(PLUGIN_NAME log_anomaly_metrics)
set(PLUGIN_ID "log_anomaly_metrics")
set(PLUGIN_VERSION "0.1.0")
set(PLUGIN_DISPLAY_NAME "Log Anomaly Metrics")
set(PLUGIN_API_VERSION "1.0.0")

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

find_package(Eigen3 QUIET)
find_package(Boost QUIET COMPONENTS regex)
find_package(Qt6 QUIET COMPONENTS Widgets Concurrent)

if(NOT Eigen3_FOUND)
    message(WARNING "Eigen3 not found. Skipping log_anomaly_metrics plugin build.")
    return()
endif()

if(NOT Boost_FOUND)
    message(WARNING "Boost not found. Skipping log_anomaly_metrics plugin build.")
    return()
endif()

if(NOT Qt6Widgets_FOUND)
    message(WARNING "Qt6 Widgets not found. Skipping log_anomaly_metrics plugin build.")
    return()
endif()

add_library(${PLUGIN_NAME} SHARED
    LogAnomalyMetricsPlugin.cpp
    LogAnomalyMetricsPlugin.hpp
    MetricsEngine.cpp
    MetricsEngine.hpp
    ConfigWidget.cpp
    ConfigWidget.hpp
    ResultsWidget.cpp
    ResultsWidget.hpp
    AnomalyChartWidget.cpp
    AnomalyChartWidget.hpp
)

set_target_properties(${PLUGIN_NAME} PROPERTIES
    AUTOMOC ON
    VERSION 0.1.0
    SOVERSION 1
    PREFIX ""
)

target_include_directories(${PLUGIN_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${Eigen3_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(${PLUGIN_NAME} PRIVATE
    application_core
    Qt6::Widgets
    Qt6::Core
    Eigen3::Eigen
    Boost::regex
)

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
# Set plugin output to build/plugins directory
set_target_properties(${PLUGIN_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

set(PLUGIN_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/config.json")
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
