# CMakeLists for error folder

# Define sources and headers for the error target
target_sources(application_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/Error.hpp>
        $<INSTALL_INTERFACE:Error.hpp>
)