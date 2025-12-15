# Field Converter Example Plugin
# Demonstrates how to create a field conversion plugin using the common IPlugin interface

set(PLUGIN_NAME field_converter_example)
set(PLUGIN_ID "field_converter_to_uppercase")
set(PLUGIN_VERSION "1.0.0")
set(PLUGIN_DISPLAY_NAME "To Uppercase Field Converter")
set(PLUGIN_API_VERSION "1.0.0")

# Create the plugin library
add_library(${PLUGIN_NAME} SHARED
    ToUpperCasePlugin.cpp
)

# Set plugin properties
set_target_properties(${PLUGIN_NAME} PROPERTIES
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
configure_file(${PLUGIN_CONFIG_TEMPLATE} ${PLUGIN_CONFIG} @ONLY)

# Copy to dist/plugins and package as zip for development
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    set(PLUGIN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/dist/plugins")
    set(PLUGIN_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")
    file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})
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
            "${PLUGIN_OUTPUT_DIR}/${PLUGIN_NAME}.zip" --format=zip
            "${PLUGIN_ENTRY}" "config.json"
        WORKING_DIRECTORY ${PLUGIN_PACKAGE_DIR}
        COMMENT "Packaging ${PLUGIN_NAME}.zip"
    )
endif()

message(STATUS "Building ${PLUGIN_NAME} plugin")
