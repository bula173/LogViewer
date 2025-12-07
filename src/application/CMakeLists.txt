message(STATUS "Building LogViewer::application core")
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

#-------------------------------------------------------------------------------
# Core library without GUI dependencies

set(APP_CORE_SRCS
    config/Config.cpp
    config/FieldTranslator.cpp
    config/BuiltinCOnversionPlugins.cpp
    config/FieldConversionPluginRegistry.cpp
    config/StringReversePlugin.cpp
    db/EventsContainer.cpp
    db/LogEvent.cpp
    filters/Filter.cpp
    filters/FilterManager.cpp
    filters/IFilterStrategy.cpp
    mvc/MainController.cpp
    parser/ParserFactory.cpp
    xml/xmlParser.cpp
    csv/CsvParser.cpp
    ai/OllamaClient.cpp
    ai/OpenAIClient.cpp
    ai/AnthropicClient.cpp
    ai/GeminiClient.cpp
    ai/AIServiceFactory.cpp
    ai/LogAnalyzer.cpp
    util/KeyEncryption.cpp
    ${CMAKE_BINARY_DIR}/src/main/version.cpp
    plugins/PluginManager.cpp
)

add_library(application_core STATIC ${APP_CORE_SRCS})

# Find CURL for AI HTTP requests
find_package(CURL REQUIRED)

target_link_libraries(application_core
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog_header_only
        expat
        CURL::libcurl
)

target_include_directories(application_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${CMAKE_CURRENT_SOURCE_DIR}/../"
        "${CMAKE_BINARY_DIR}/src/main"
        "${CMAKE_SOURCE_DIR}/src/main"
)

target_include_directories(application_core SYSTEM PUBLIC
    ${nlohmann_json_SOURCE_DIR}/include
    ${spdlog_SOURCE_DIR}/include
    ${EXPAT_INCLUDE_DIRS})

target_precompile_headers(application_core
    PUBLIC
        <nlohmann/json.hpp>
        <spdlog/spdlog.h>)

add_subdirectory(ui)
