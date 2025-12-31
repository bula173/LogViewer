

target_sources(application_core
    PRIVATE
        PluginManager.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/PluginManager.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IAIPlugin.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IAnalysisPlugin.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IFieldConversionPlugin.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IPlugin.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IPluginObserver.hpp>
        $<INSTALL_INTERFACE:PluginManager.hpp>
)
