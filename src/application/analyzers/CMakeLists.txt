target_sources(application_core
    PRIVATE ActorDiscoverer.cpp
            SequenceMessages.cpp
            TestOutline.cpp
            TestRunIndex.cpp
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ActorDiscoverer.hpp>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/SequenceMessages.hpp>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/TestOutline.hpp>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/TestRunIndex.hpp>
            $<INSTALL_INTERFACE:ActorDiscoverer.hpp>
            $<INSTALL_INTERFACE:SequenceMessages.hpp>
            $<INSTALL_INTERFACE:TestOutline.hpp>
            $<INSTALL_INTERFACE:TestRunIndex.hpp>)
