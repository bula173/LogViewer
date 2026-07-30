# CMakeLists for filters folder

# Define sources and headers for the filters target
target_sources(application_core
    PRIVATE
        Filter.cpp
        FilterManager.cpp
        FilterOptimizer.cpp
        IFilterStrategy.cpp
        FilterSerializer.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/Filter.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/FilterManager.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/FilterOptimizer.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IFilterStrategy.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/FilterSerializer.hpp>
        $<INSTALL_INTERFACE:Filter.hpp>
        $<INSTALL_INTERFACE:FilterManager.hpp>
        $<INSTALL_INTERFACE:FilterOptimizer.hpp>
        $<INSTALL_INTERFACE:IFilterStrategy.hpp>
        $<INSTALL_INTERFACE:FilterSerializer.hpp>
)