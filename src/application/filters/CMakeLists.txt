# CMakeLists for filters folder

# Define sources and headers for the filters target
target_sources(application_core
    PRIVATE
        Filter.cpp
        FilterManager.cpp
        IFilterStrategy.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/Filter.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/FilterManager.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IFilterStrategy.hpp>
        $<INSTALL_INTERFACE:Filter.hpp>
        $<INSTALL_INTERFACE:FilterManager.hpp>
        $<INSTALL_INTERFACE:IFilterStrategy.hpp>
)