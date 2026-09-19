target_sources(application_core
    PRIVATE
        SapiLogParser.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/SapiLogParser.hpp>
        $<INSTALL_INTERFACE:SapiLogParser.hpp>
)
