cmake_minimum_required(VERSION 3.22)
project(BasicExamplePlugin)

# Plugin metadata
set(PLUGIN_NAME "basic_example")
set(PLUGIN_ID "basic_example")
set(PLUGIN_VERSION "1.0.0")
set(PLUGIN_DISPLAY_NAME "Basic Example")
set(PLUGIN_API_VERSION "1.0.0")

# Find LogViewer SDK
# The host application should set CMAKE_PREFIX_PATH to the SDK directory
# e.g.: cmake -DCMAKE_PREFIX_PATH=/path/to/LogViewer_install/sdk/lib/cmake
find_package(LogViewer CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Core REQUIRED)

# Create the plugin library as a shared module
add_library(${PLUGIN_NAME} MODULE
    src/BasicPlugin.cpp
)

# Link against the plugin API
# The plugin_api is header-only and provides the C-ABI interface definitions
target_link_libraries(${PLUGIN_NAME}
    PRIVATE
        LogViewer::plugin_api
        Qt6::Core
)

# Set output properties
set_target_properties(${PLUGIN_NAME} PROPERTIES
    PREFIX ""  # No "lib" prefix on any platform
)

# Platform-specific configuration
if(APPLE)
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        SUFFIX ".dylib"
    )
    set(PLUGIN_EXTENSION ".dylib")
elseif(WIN32)
    set(PLUGIN_EXTENSION ".dll")
else()
    set(PLUGIN_EXTENSION ".so")
endif()

# Generate plugin configuration file (optional but recommended)
set(PLUGIN_ENTRY "${PLUGIN_NAME}${PLUGIN_EXTENSION}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.json
    @ONLY
)

# Copy config.json to the same directory as the plugin DLL
add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/config.json
        $<TARGET_FILE_DIR:${PLUGIN_NAME}>/config.json
    COMMENT "Copying config.json to plugin directory"
)

# Collect Runtime Dependencies (DLLs/SOs/dylibs)
# For Windows/MinGW, copy all required Qt and runtime DLLs
# This makes the plugin self-contained for distribution
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")

if(WIN32)
    # On Windows, collect all runtime dependencies
    # This includes Qt6 DLLs, MinGW runtime DLLs, etc.
    # Uses LogViewer SDK's copy_dlls.cmake helper script
    
    # Try to find copy_dlls.cmake in SDK path, or fall back to source tree
    get_filename_component(LOGVIEWER_CMAKE_DIR "${LogViewer_CONFIG}" DIRECTORY)
    if(NOT EXISTS "${LOGVIEWER_CMAKE_DIR}/copy_dlls.cmake")
        # Fall back to source tree when building as part of main project
        set(LOGVIEWER_CMAKE_DIR "${CMAKE_SOURCE_DIR}/cmake")
    endif()
    
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DARGV0="$<TARGET_FILE:${PLUGIN_NAME}>"
            -DARGV1="${CMAKE_CURRENT_BINARY_DIR}/lib"
            -DARGV2="${CMAKE_C_COMPILER}"
            -P "${LOGVIEWER_CMAKE_DIR}/copy_dlls.cmake"
        COMMENT "Collecting runtime dependencies for ${PLUGIN_NAME}"
    )
endif()

# Create ZIP archive for distribution
# The ZIP archive contains:
# - Plugin binary (.dll/.so/.dylib)
# - config.json (metadata)
# - lib/ directory (all runtime dependencies - DLLs on Windows, etc.)
# This makes the plugin self-contained for end-user installation

set(PLUGIN_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")
set(PLUGIN_PACKAGE_NAME "${PLUGIN_NAME}.zip")
set(PLUGIN_PACKAGE_PATH "${PLUGIN_PACKAGE_DIR}/${PLUGIN_PACKAGE_NAME}")

file(MAKE_DIRECTORY "${PLUGIN_PACKAGE_DIR}")

# Create the ZIP archive after the plugin is built and dependencies collected
if(WIN32)
    # On Windows, include collected dependencies in the archive
    add_custom_command(
        OUTPUT "${PLUGIN_PACKAGE_PATH}"
        COMMAND ${CMAKE_COMMAND} -E tar "cf" "${PLUGIN_PACKAGE_PATH}"
            --format=zip
            "$<TARGET_FILE:${PLUGIN_NAME}>"
            "${CMAKE_CURRENT_BINARY_DIR}/config.json"
            "lib/"
        COMMENT "Creating plugin archive with dependencies: ${PLUGIN_PACKAGE_NAME}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS ${PLUGIN_NAME} "${CMAKE_CURRENT_BINARY_DIR}/config.json"
    )
else()
    # On other platforms, just the binary and config (no lib/ directory)
    add_custom_command(
        OUTPUT "${PLUGIN_PACKAGE_PATH}"
        COMMAND ${CMAKE_COMMAND} -E tar "cf" "${PLUGIN_PACKAGE_PATH}"
            --format=zip
            "$<TARGET_FILE:${PLUGIN_NAME}>"
            "${CMAKE_CURRENT_BINARY_DIR}/config.json"
        COMMENT "Creating plugin archive: ${PLUGIN_PACKAGE_NAME}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS ${PLUGIN_NAME} "${CMAKE_CURRENT_BINARY_DIR}/config.json"
    )
endif()

# Custom target to create/rebuild the archive
add_custom_target(package_plugin ALL
    DEPENDS "${PLUGIN_PACKAGE_PATH}"
    COMMENT "Plugin package: ${PLUGIN_PACKAGE_PATH}"
)

message(STATUS "BasicExamplePlugin: Configured")
message(STATUS "  Plugin: ${PLUGIN_NAME}")
message(STATUS "  Version: ${PLUGIN_VERSION}")
message(STATUS "  Output: $<TARGET_FILE:${PLUGIN_NAME}>")
message(STATUS "  Package: ${PLUGIN_PACKAGE_PATH}")
if(WIN32)
    message(STATUS "  Dependencies: Included in lib/ directory")
endif()
