# LogViewer Plugin Examples
# Build example plugins to demonstrate SDK usage

# Only build examples if SDK is built and we're not in subproject mode
if(BUILD_SDK)
    # When building as part of the main project, provide SDK paths directly
    # so examples can find the SDK before it's installed
    set(CMAKE_PREFIX_PATH 
        "${CMAKE_BINARY_DIR}/src/plugin_api"
        "${CMAKE_CURRENT_LIST_DIR}/../src/plugin_api"
        "${CMAKE_PREFIX_PATH}"
    )
    
    add_subdirectory(BasicPlugin)
    message(STATUS "LogViewer Examples: BasicPlugin included")
endif()
