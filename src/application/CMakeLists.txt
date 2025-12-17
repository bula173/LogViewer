message(STATUS "Building LogViewer::application core")
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

#-------------------------------------------------------------------------------
# Core library without GUI dependencies

set(APP_CORE_SRCS
    config/Config.cpp
    config/FieldTranslator.cpp
    config/BuiltinConversionPlugins.cpp
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
    ../plugins/ai/OllamaClient.cpp
    ../plugins/ai/OpenAIClient.cpp
    ../plugins/ai/AnthropicClient.cpp
    ../plugins/ai/GeminiClient.cpp
    ../plugins/ai/AIServiceFactory.cpp
    ../plugins/ai/LogAnalyzer.cpp
    util/KeyEncryption.cpp
    ${CMAKE_BINARY_DIR}/src/main/version.cpp
    plugins/PluginManager.cpp
)

add_library(application_core STATIC ${APP_CORE_SRCS})

# Find CURL for AI HTTP requests
find_package(CURL REQUIRED)
# Find libarchive for ZIP support
find_package(LibArchive CONFIG QUIET)
if (NOT LibArchive_FOUND)
    find_package(LibArchive REQUIRED)
endif()

target_link_libraries(application_core
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog_header_only
        expat
        CURL::libcurl
        LibArchive::LibArchive
)

target_include_directories(application_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${CMAKE_CURRENT_SOURCE_DIR}/../"
        "${CMAKE_SOURCE_DIR}/src/plugins"
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

# Create symlinks for plugin API headers in include directory
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/include/application/plugins")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/include/application/parser")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/include/application/db")

# List of headers needed by plugins
set(PLUGIN_API_HEADERS
    # Plugin interfaces
    "plugins/IPlugin.hpp"
    "plugins/IAnalysisPlugin.hpp"
    "plugins/IFieldConversionPlugin.hpp"
    "plugins/IAIPlugin.hpp"
    "plugins/IPluginObserver.hpp"
    # Parser interface
    "parser/IDataParser.hpp"
    # Database/Event interfaces
    "db/LogEvent.hpp"
    "db/EventsContainer.hpp"
)

# Create symlinks (or copy on Windows) for plugin development headers
if(UNIX)
    foreach(HEADER ${PLUGIN_API_HEADERS})
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_CURRENT_SOURCE_DIR}/${HEADER}"
            "${CMAKE_SOURCE_DIR}/include/application/${HEADER}"
        )
    endforeach()
else()
    # Windows: use file copy
    foreach(HEADER ${PLUGIN_API_HEADERS})
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/${HEADER}"
            "${CMAKE_SOURCE_DIR}/include/application/${HEADER}"
            COPYONLY
        )
    endforeach()
endif()

add_subdirectory(ui)
