message(STATUS "Building LogViewer::thirdparty")
message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# Ensure git submodules are checked out
execute_process(COMMAND git submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(wxUSE_METAL OFF CACHE BOOL "Disable METAL build" FORCE)
set(wxUSE_OPENGL ON CACHE BOOL "Enable OpenGL support" FORCE)


find_package(wxWidgets QUIET COMPONENTS core base xml)

if(wxWidgets_FOUND)
    message(STATUS "wxWidgets Found in system!")
    set(WX_LOCAL_BUILD TRUE CACHE BOOL "WxLocal build")
else()
    message(STATUS "wxWidgets not found! Try to build it from source.")
    # Set wxWidgets to static mode
    set(wxBUILD_SHARED OFF CACHE BOOL "Build static libraries" FORCE)
    set(wxBUILD_PRECOMP OFF CACHE BOOL "Disable precompiled headers" FORCE)
    set(wxBUILD_MONOLITHIC OFF CACHE BOOL "Disable monolithic build" FORCE)
    set(wxBUILD_FEATURES "core;xml;base" CACHE STRING "Build only base, core, and xml libraries" FORCE)
    set(wxUSE_XRC OFF CACHE BOOL "Disable XRC" FORCE)
    set(wxUSE_AUI OFF CACHE BOOL "Disable AUI" FORCE)
    set(wxUSE_SYS_LIBS OFF CACHE BOOL "Don't use system libraries" FORCE)
    set(wxUSE_WEBVIEW OFF CACHE BOOL "Disable webview" FORCE)
    add_subdirectory(wxwidgets)

    set(WX_CONFIG_EXECUTABLE "${wxWidgets_BINARY_DIR}/wx-config")
    set(ENV{WX_CONFIG} "${WX_CONFIG_EXECUTABLE}")

    # Custom target to trigger the build and setup wxWidgets properly
    add_custom_target(BuildWxWidgets ALL
    COMMENT "Building wxWidgets locally"
    )

    add_dependencies(BuildWxWidgets wxcore wxbase)

    #Display all CMake variables
    #include(${CMAKE_SOURCE_DIR}/displayVars.cmake)
    set(WX_LOCAL_BUILD TRUE CACHE BOOL "WxLocal build")

endif()

add_subdirectory(gflags)
add_subdirectory(gtest)


