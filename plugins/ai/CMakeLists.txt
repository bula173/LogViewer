set(PLUGIN_NAME ai_provider)
set(PLUGIN_ID "ai_provider")
set(PLUGIN_VERSION "1.0.0")
set(PLUGIN_DISPLAY_NAME "AI Provider")
set(PLUGIN_API_VERSION "1.0.0")

find_package(CURL REQUIRED)  # Changed to uppercase

add_library(${PLUGIN_NAME} SHARED
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

target_link_libraries(${PLUGIN_NAME}
    PRIVATE
        application_core
        CURL::libcurl  # Changed to standard target name
)

set_target_properties(${PLUGIN_NAME} PROPERTIES
    AUTOMOC ON
    VERSION 1.0.0
    SOVERSION 1
    PREFIX ""
)

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

install(FILES
        "${CMAKE_BINARY_DIR}/plugins/${PLUGIN_NAME}.zip"
        DESTINATION .
        COMPONENT plugins
)

message(STATUS "Building ${PLUGIN_NAME} plugin")
