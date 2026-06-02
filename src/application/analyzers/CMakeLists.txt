target_sources(application_core
    PRIVATE ActorDiscoverer.cpp
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ActorDiscoverer.hpp>
            $<INSTALL_INTERFACE:ActorDiscoverer.hpp>)
