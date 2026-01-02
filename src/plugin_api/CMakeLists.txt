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
    # Install only the public C API headers and SDK helper for third-party plugin developers
    install(FILES
        ${CMAKE_CURRENT_LIST_DIR}/PluginC.h
        ${CMAKE_CURRENT_LIST_DIR}/PluginAIProviderC.h
        ${CMAKE_CURRENT_LIST_DIR}/PluginTypesC.h
        ${CMAKE_CURRENT_LIST_DIR}/PluginEventsC.h
        ${CMAKE_CURRENT_LIST_DIR}/PluginLoggerC.h
        ${CMAKE_CURRENT_LIST_DIR}/PluginKeyEncryptionC.h
        # AI/Analysis/FieldConversion helpers merged into PluginC.h; SDK helper removed
        DESTINATION sdk/include
        COMPONENT sdk
    )

    # Configure and install CMake helper files so SDK consumers can find the
    # plugin API target via find_package(LogViewer CONFIG REQUIRED)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/LogViewerPluginTargets.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewerPluginTargets.cmake @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/LogViewerPluginTargets.cmake
        DESTINATION sdk/lib/cmake/
        COMPONENT sdk
    )

    configure_file(${CMAKE_SOURCE_DIR}/cmake/LogViewerConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewerConfig.cmake @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/LogViewerConfig.cmake
        DESTINATION sdk/lib/cmake/
        COMPONENT sdk
    )
    
    # Configure and install version file for find_package version checks
    configure_file(${CMAKE_SOURCE_DIR}/cmake/LogViewerConfigVersion.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewerConfigVersion.cmake @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/LogViewerConfigVersion.cmake
        DESTINATION sdk/lib/cmake/
        COMPONENT sdk
    )
endif()
