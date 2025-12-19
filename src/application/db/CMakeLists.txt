# CMakeLists for db folder

# Define sources and headers for the db target
target_sources(application_core
    PRIVATE
        EventsContainer.cpp
        LogEvent.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/EventsContainer.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/LogEvent.hpp>
        $<INSTALL_INTERFACE:EventsContainer.hpp>
        $<INSTALL_INTERFACE:LogEvent.hpp>
)