set(PLUGIN_NAME ai_provider)
set(PLUGIN_ID "ai_provider")
set(PLUGIN_VERSION "1.0.0")
set(PLUGIN_DISPLAY_NAME "AI Provider")
set(PLUGIN_API_VERSION "1.0.0")

find_package(CURL REQUIRED)
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)



add_library(${PLUGIN_NAME} MODULE
    AIProviderPlugin.cpp
    AIProviderPlugin.hpp
    AIAnalysisPanel.cpp
    AIAnalysisPanel.hpp
    AIChatPanel.cpp
    AIChatPanel.hpp
    AIConfigPanel.cpp
    AIConfigPanel.hpp
    AIServiceFactory.cpp
    AIServiceFactory.hpp
    LogAnalyzer.cpp
    LogAnalyzer.hpp
    LLMClient.hpp
    IAIService.hpp
    OllamaClient.cpp
    OllamaClient.hpp
    OllamaSetupDialog.cpp
    OllamaSetupDialog.hpp
    OpenAIClient.cpp
    OpenAIClient.hpp
    GeminiClient.cpp
    GeminiClient.hpp
    AnthropicClient.cpp
    AnthropicClient.hpp
)

# If building in-tree with the main LogViewer target available, enable internal interface support
message(STATUS "ai_provider: Building SDK-first (no application internals)")

# Ensure JSON include is available for `IAIPlugin.hpp` and other headers
target_link_libraries(${PLUGIN_NAME}
    PRIVATE
    nlohmann_json::nlohmann_json
    fmt::fmt-header-only
    Qt6::Widgets
)

if(TARGET plugin_api)
    target_link_libraries(${PLUGIN_NAME}
        PRIVATE
            CURL::libcurl
            plugin_api
    )
elseif(TARGET LogViewer::plugin_api)
    target_link_libraries(${PLUGIN_NAME}
        PRIVATE
            CURL::libcurl
            LogViewer::plugin_api
    )
else()
    # Require installed SDK when no in-tree plugin_api target is available
    find_package(LogViewer CONFIG REQUIRED)
    target_link_libraries(${PLUGIN_NAME}
        PRIVATE
            CURL::libcurl
            LogViewer::plugin_api
    )
endif()

set_target_properties(${PLUGIN_NAME} PROPERTIES
    AUTOMOC ON
    PREFIX ""
)

if(APPLE)
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        SUFFIX ".dylib"
    )
endif()

if(WIN32)
    set(PLUGIN_EXTENSION ".dll")
elseif(APPLE)
    set(PLUGIN_EXTENSION ".dylib")
else()
    set(PLUGIN_EXTENSION ".so")
endif()

# Revert to the original configuration for generating the config.json file
set(PLUGIN_ENTRY "${PLUGIN_NAME}${PLUGIN_EXTENSION}")
set(PLUGIN_CONFIG_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/config.json.in")
set(PLUGIN_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/config.json")

configure_file(${PLUGIN_CONFIG_TEMPLATE} ${PLUGIN_CONFIG} @ONLY)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")

if(WIN32)
    # Determine main executable path for exclusion if available (no application_core usage)
    if(TARGET LogViewer)
        set(PLUGIN_EXCLUDE_APP_FILE "$<TARGET_FILE:LogViewer>")
    else()
        set(PLUGIN_EXCLUDE_APP_FILE "")
    endif()

    # Copy to lib for packaging (uses SDK-only or in-tree `LogViewer::plugin_api`, never application_core)
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DARGV0="$<TARGET_FILE:${PLUGIN_NAME}>"
            -DARGV1="$<TARGET_FILE_DIR:${PLUGIN_NAME}>/lib"
            -DARGV2="${CMAKE_C_COMPILER}"
            -DARGV3="${PLUGIN_EXCLUDE_APP_FILE}"
            -P ${PROJECT_SOURCE_DIR}/cmake/copy_dlls.cmake
        COMMENT "Collecting MinGW runtime DLLs to lib"
    )
endif()

# Define the output directory for the ZIP archive
set(PACKAGE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")
set(PACKAGE_ARCHIVE "${PACKAGE_OUTPUT_DIR}/ai_provider.zip")

# Ensure the output directory exists
file(MAKE_DIRECTORY "${PACKAGE_OUTPUT_DIR}")
message(STATUS "Package output directory: ${PACKAGE_OUTPUT_DIR}")

# Create the ZIP archive
add_custom_command(
    OUTPUT ${PACKAGE_ARCHIVE}
    DEPENDS ${PLUGIN_NAME} ${PLUGIN_CONFIG}
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${PACKAGE_ARCHIVE}" --format=zip
        $<TARGET_FILE_DIR:${PLUGIN_NAME}>/config.json
        $<TARGET_FILE:${PLUGIN_NAME}>
        $<TARGET_FILE_DIR:${PLUGIN_NAME}>/lib
    COMMENT "Creating ZIP archive for ai_provider"
    VERBATIM
)

# Custom target to build the ZIP archive
add_custom_target(create_ai_provider_zip ALL
    DEPENDS ${PACKAGE_ARCHIVE}
    COMMENT "Building ZIP archive for ai_provider"
)


install (FILES ${PACKAGE_ARCHIVE}
    DESTINATION plugins
    COMPONENT plugins
)

# `cmake --install` does not build targets. Ensure the plugin ZIP exists when
# installing the plugins component (e.g. in CI staging installs).
install(CODE "
    if(NOT EXISTS \"${PACKAGE_ARCHIVE}\")
        message(STATUS \"ai_provider: ZIP not found, building create_ai_provider_zip before install\")
        execute_process(
            COMMAND \"${CMAKE_COMMAND}\" --build \"${CMAKE_BINARY_DIR}\" --target create_ai_provider_zip
            RESULT_VARIABLE _zip_build_res
        )
        if(NOT _zip_build_res EQUAL 0)
            message(FATAL_ERROR \"ai_provider: Failed to build ZIP (create_ai_provider_zip)\")
        endif()
    endif()
" COMPONENT plugins)

# Copy plugin files to main executable's plugins directory for development builds
add_custom_target(copy_ai_provider_to_main ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/src/main/plugins/ai_provider
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/config.json ${CMAKE_BINARY_DIR}/src/main/plugins/ai_provider/
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ai_provider> ${CMAKE_BINARY_DIR}/src/main/plugins/ai_provider/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/lib ${CMAKE_BINARY_DIR}/src/main/plugins/ai_provider/lib
    DEPENDS ai_provider ${PLUGIN_CONFIG}
    COMMENT "Copying ai_provider plugin to main plugins directory"
)


