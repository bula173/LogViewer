message(STATUS "Building LogViewer::application")
message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# Ensure git submodules are checked out
execute_process(COMMAND git submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Add wxWidgets as a subdirectory
set(wxWidgets_BUILD_TESTS OFF CACHE BOOL "Disable wxWidgets tests" FORCE)
set(wxBUILD_SHARED OFF CACHE BOOL "Build wxWidgets as static library" FORCE)
set(wxBUILD_INSTALL ON CACHE BOOL "Install wxWidgets" FORCE)


find_package(wxWidgets QUIET COMPONENTS core base xml)

if(wxWidgets_FOUND)
    message(STATUS "wxWidgets Found!")
    message(STATUS "wxWidgets include directories: ${wxWidgets_INCLUDE_DIRS}")
    message(STATUS "wxWidgets libraries: ${wxWidgets_LIBRARIES}")
    message(STATUS "wxWidgets use file: ${wxWidgets_USE_FILE}")
    message(STATUS "wxWidgets root: ${wxWidgets_ROOT}")
else()
    message(STATUS "wxWidgets not found! Try to build it from source.")

    add_subdirectory(wxwidgets)

    set(wxWidgets_ROOT ${wxWidgets_BINARY_DIR} CACHE STRING "wxWidgets root directory" FORCE)
    set(WX_CONFIG_PATH ${wxWidgets_BINARY_DIR}/wx-config CACHE STRING "wx-config path" FORCE)

    # Check if wx-config exists
    if(EXISTS ${WX_CONFIG_PATH})
        message(STATUS "wx-config found at ${WX_CONFIG_PATH}")

        # Get wxWidgets compilation flags using wx-config
        execute_process(
            COMMAND ${WX_CONFIG_PATH} --cxxflags
            OUTPUT_VARIABLE wxWidgets_CXX_FLAGS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        execute_process(
            COMMAND ${WX_CONFIG_PATH} --libs
            OUTPUT_VARIABLE wxWidgets_LIBS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        # Extract include directories from wxWidgets_CXX_FLAGS
        string(REGEX MATCHALL "-I[^ ]+" wxWidgets_INCLUDE_DIRS ${wxWidgets_CXX_FLAGS})
        string(REPLACE "-I" ";" wxWidgets_INCLUDE_DIRS ${wxWidgets_INCLUDE_DIRS})

        # Extract library directories from wxWidgets_LIBS
        string(REGEX MATCHALL "-L[^ ]+" wxWidgets_LIBRARY_DIRS ${wxWidgets_LIBS})

        # Extract library names from wxWidgets_LIBS
        string(REGEX MATCHALL "-l[^ ]+" wxWidgets_LIBRARIES ${wxWidgets_LIBS})

        # Extract frameworks from wxWidgets_LIBS
        string(REGEX MATCHALL "-framework [^ ]+" wxWidgets_FRAMEWORKS ${wxWidgets_LIBS})

        message(STATUS "wxWidgets include directories: ${wxWidgets_INCLUDE_DIRS}")
        message(STATUS "wxWidgets library directories: ${wxWidgets_LIBRARY_DIRS}")
        message(STATUS "wxWidgets libraries: ${wxWidgets_LIBRARIES}")
        message(STATUS "wxWidgets frameworks: ${wxWidgets_FRAMEWORKS}")

        # Set include and library directories for the project
        set(wxWidgets_INCLUDE_DIRS ${wxWidgets_INCLUDE_DIRS} CACHE INTERNAL "wxWidgets_INCLUDE_DIRS")
        set(wxWidgets_LIBRARIES ${wxWidgets_LIBRARY_DIRS} ${wxWidgets_LIBRARIES} ${wxWidgets_FRAMEWORKS} CACHE INTERNAL "wxWidgets_LIBRARIES")


    else()
        message(FATAL_ERROR "wx-config not found at ${WX_CONFIG_PATH}")
    endif()
endif()

add_subdirectory(gflags)
add_subdirectory(gtest)
