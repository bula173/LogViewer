target_sources(application_core
    PRIVATE
        JsonParser.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/JsonParser.hpp>
        $<INSTALL_INTERFACE:JsonParser.hpp>
)
