target_sources(application_core
    PRIVATE
        CsvParser.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/CsvParser.hpp>
        $<INSTALL_INTERFACE:CsvParser.hpp>
)