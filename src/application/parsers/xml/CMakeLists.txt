target_sources(application_core
    PRIVATE
        xmlParser.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/xmlParser.hpp>
        $<INSTALL_INTERFACE:xmlParser.hpp>
)