## CMake config for plugin_api SDK headers and implementation

# Plugin API is header-only INTERFACE library
add_library(plugin_api INTERFACE)

target_include_directories(plugin_api
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Provide an alias used by plugins when linking against an installed SDK
add_library(LogViewer::plugin_api ALIAS plugin_api)

message(STATUS "plugin_api: INTERFACE library target created (header-only)")

if(BUILD_SDK)
    # Export config files to build tree (for FetchContent usage)
    # This makes find_package(LogViewer) work automatically when using FetchContent
    # without requiring manual CMAKE_PREFIX_PATH configuration
    export(PACKAGE LogViewer)
    
    # Also make them discoverable in the build directory
    configure_file(${PROJECT_SOURCE_DIR}/cmake/LogViewerPluginTargets.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewerPluginTargets.cmake @ONLY)
    
    configure_file(${PROJECT_SOURCE_DIR}/cmake/LogViewerConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewerConfig.cmake @ONLY)
    
    configure_file(${PROJECT_SOURCE_DIR}/cmake/LogViewerPluginPackaging.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewerPluginPackaging.cmake @ONLY)
    
    configure_file(${PROJECT_SOURCE_DIR}/cmake/LogViewerConfigVersion.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewerConfigVersion.cmake @ONLY)
    
    file(COPY
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewerPluginTargets.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewerConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewerConfigVersion.cmake
        DESTINATION ${CMAKE_BINARY_DIR}/src/plugin_api
    )

    # Only install SDK when building as standalone project (not as subproject)
    # When used as subproject (e.g., via FetchContent), only the build-tree targets are available
    if(NOT LOGVIEWER_SUBPROJECT)
        # Install public C API headers
        install(FILES
            ${CMAKE_CURRENT_LIST_DIR}/PluginC.h
            ${CMAKE_CURRENT_LIST_DIR}/PluginAIProviderC.h
            ${CMAKE_CURRENT_LIST_DIR}/PluginTypesC.h
            ${CMAKE_CURRENT_LIST_DIR}/PluginEventsC.h
            ${CMAKE_CURRENT_LIST_DIR}/PluginHostUiC.h
            ${CMAKE_CURRENT_LIST_DIR}/PluginLoggerC.h
            ${CMAKE_CURRENT_LIST_DIR}/PluginKeyEncryptionC.h
            DESTINATION sdk/include
            COMPONENT sdk
        )

        # Install config files for the installed SDK
        install(FILES 
            ${CMAKE_CURRENT_BINARY_DIR}/LogViewerPluginTargets.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/LogViewerConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/LogViewerConfigVersion.cmake
            DESTINATION sdk/lib/cmake/
            COMPONENT sdk
        )

        # Optional helper for packaging plugins into .zip archives
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/LogViewerPluginPackaging.cmake
            DESTINATION sdk/lib/cmake/
            COMPONENT sdk
        )

        # Install helper script for collecting runtime dependencies (DLLs)
        install(FILES ${PROJECT_SOURCE_DIR}/cmake/copy_dlls.cmake
            DESTINATION sdk/lib/cmake/
            COMPONENT sdk
        )
    endif()
endif()
