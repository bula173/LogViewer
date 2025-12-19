# CMakeLists for util folder

# Define sources and headers for the util target
target_sources(application_core
    PRIVATE
        KeyEncryption.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/KeyEncryption.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/Logger.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/Result.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/ThirdPartyWarnings.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/WxWidgetsUtils.hpp>
        $<INSTALL_INTERFACE:KeyEncryption.hpp>
        $<INSTALL_INTERFACE:Logger.hpp>
        $<INSTALL_INTERFACE:Result.hpp>
        $<INSTALL_INTERFACE:ThirdPartyWarnings.hpp>
        $<INSTALL_INTERFACE:WxWidgetsUtils.hpp>
)