message(STATUS "Building LogViewer::thirdparty")
message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# Enable FetchContent or Git submodule builds
include(FetchContent)

find_package(wxWidgets QUIET COMPONENTS core base xml)

if(wxWidgets_FOUND)
    message(STATUS "wxWidgets Found in system!")
    set(WX_LOCAL_BUILD FALSE CACHE BOOL "WxLocal build")
else()
    message(STATUS "wxWidgets not found! Try to build it from source.")

    # Ensure git submodules are checked out
    execute_process(COMMAND git submodule update --init --recursive
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    # Set wxWidgets to static mode
    set(wxUSE_SHARED OFF CACHE BOOL "Build static libraries" FORCE)

    # Disable unnecessary libraries
    set(wxUSE_WEBVIEW "no" CACHE STRING "Disable wxWebView library" FORCE)
    set(wxUSE_QA "no" CACHE STRING "Disable wxWebView library" FORCE)
    set(wxUSE_AUI "no" CACHE STRING "Disable wxWebView library" FORCE)
    set(wxUSE_NET "no" CACHE STRING "Disable wxWebView library" FORCE)
    set(wxUSE_MEDIA "no" CACHE STRING "Disable wxMedia library" FORCE)

    # Enable required libraries
    set(wxUSE_BASE "yes" CACHE STRING "Enable wxBase library" FORCE)
    set(wxUSE_CORE "yes" CACHE STRING "Enable wxCore library" FORCE)
    #without xrc a xml is not enabled
    set(wxUSE_XRC "yes" CACHE STRING "Disable wxXRC library" FORCE)

    # Disable system libraries and liblzma
    set(wxUSE_USE_LIBLZMA "no" CACHE STRING "Disable liblzma usage" FORCE)
    set(wxUSE_USE_SYSTEM_LIBRARIES "no" CACHE STRING "Disable usage of system libraries" FORCE)

    if(APPLE)
        # macOS-specific options
        set(wxUSE_OSX_COCOA "yes" CACHE STRING "Use Cocoa on macOS" FORCE)
        set(wxUSE_USE_PRECOMP_HEADERS "no" CACHE STRING "Disable precompiled headers" FORCE)
        set(wxUSE_WITHOUT_SUBDIRS "yes" CACHE STRING "Place all libraries in the same directory" FORCE)
    endif()
    
    add_subdirectory(wxwidgets)

    set(WX_CONFIG_EXECUTABLE "${wxWidgets_BINARY_DIR}/wx-config")
    set(ENV{WX_CONFIG} "${WX_CONFIG_EXECUTABLE}")

    # Custom target to trigger the build and setup wxWidgets properly
    add_custom_target(BuildWxWidgets ALL
    COMMENT "Building wxWidgets locally"
    )

    add_dependencies(BuildWxWidgets wxcore wxbase wxxrc)

    #Display all CMake variables
    #include(${CMAKE_SOURCE_DIR}/displayVars.cmake)
    set(WX_LOCAL_BUILD TRUE CACHE BOOL "WxLocal build")

endif()

#add_subdirectory(gflags)
add_subdirectory(gtest)


