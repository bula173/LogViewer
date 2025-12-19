# CMakeLists for mvc folder

# Define sources and headers for the mvc target
target_sources(application_core
    PRIVATE
        MainController.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IControler.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IModel.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/IView.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/MainController.hpp>
        $<INSTALL_INTERFACE:IControler.hpp>
        $<INSTALL_INTERFACE:IModel.hpp>
        $<INSTALL_INTERFACE:IView.hpp>
        $<INSTALL_INTERFACE:MainController.hpp>
)