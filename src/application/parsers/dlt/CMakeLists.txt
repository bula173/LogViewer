target_sources(application_core
    PRIVATE
        DltParser.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/DltParser.hpp>
        $<INSTALL_INTERFACE:DltParser.hpp>
)
