# Field Converter Example Plugin
# Demonstrates how to create a field conversion plugin using the common IPlugin interface

set(PLUGIN_NAME field_converter_example)

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

# Copy to dist/plugins for development
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    set(PLUGIN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/dist/plugins")
    file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})

    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${PLUGIN_NAME}>
            ${PLUGIN_OUTPUT_DIR}/field_converter_example.dylib
        COMMENT "Copying ${PLUGIN_NAME} to ${PLUGIN_OUTPUT_DIR}"
    )
endif()

message(STATUS "Building ${PLUGIN_NAME} plugin")
