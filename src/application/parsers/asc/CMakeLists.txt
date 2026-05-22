target_sources(application_core
    PRIVATE
        AscParser.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/AscParser.hpp>
        $<INSTALL_INTERFACE:AscParser.hpp>
)
