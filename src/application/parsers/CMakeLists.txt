# CMakeLists for parsers folder

add_subdirectory(csv)
add_subdirectory(json)
add_subdirectory(xml)
add_subdirectory(dbc)
add_subdirectory(asc)
add_subdirectory(dlt)
add_subdirectory(evlog)

# Define sources and headers for the parsers target
target_sources(application_core
    PRIVATE
        ParserFactory.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IDataParser.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ParserFactory.hpp>
        $<INSTALL_INTERFACE:IDataParser.hpp>
        $<INSTALL_INTERFACE:ParserFactory.hpp>
)