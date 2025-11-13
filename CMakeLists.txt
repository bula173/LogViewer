cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(LogViewer VERSION 0.0.5 DESCRIPTION "A simple log viewer" LANGUAGES CXX)

cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands for clang-tidy and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(useGoldLinker)

#------------------------------------------------------------------------------
# Build options

option(MEASURE_ALL "When enabled all commands will be passed through time command" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy during compilation" OFF)
option(ENABLE_COVERAGE "Enable code coverage (Debug builds only)" OFF)

if(MEASURE_ALL)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "time")
endif()

#------------------------------------------------------------------------------
# ccache support for faster rebuilds (2-5x improvement)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache: ENABLED (${CCACHE_PROGRAM})")
else()
    message(STATUS "ccache: NOT FOUND (install for faster rebuilds)")
endif()

#------------------------------------------------------------------------------
# Link-Time Optimization (LTO) for Release builds (10-30% smaller binaries)

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)

if(IPO_SUPPORTED AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    message(STATUS "Link-Time Optimization (LTO): ENABLED")
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(STATUS "Link-Time Optimization (LTO): DISABLED (${IPO_ERROR})")
    else()
        message(STATUS "Link-Time Optimization (LTO): DISABLED (only for Release builds)")
    endif()
endif()

#------------------------------------------------------------------------------
# clang-tidy integration (optional)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY 
            ${CLANG_TIDY_EXE};
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
        )
        message(STATUS "clang-tidy: ENABLED at compile time (${CLANG_TIDY_EXE})")
    else()
        message(WARNING "clang-tidy: REQUESTED but NOT FOUND")
    endif()
else()
    message(STATUS "clang-tidy: DISABLED (use -DENABLE_CLANG_TIDY=ON to enable)")
endif()

#------------------------------------------------------------------------------
# Code coverage support (Debug builds only)

if(ENABLE_COVERAGE)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
            add_link_options(--coverage)
            message(STATUS "Code coverage: ENABLED")
        else()
            message(WARNING "Code coverage: REQUESTED but not supported by ${CMAKE_CXX_COMPILER_ID}")
        endif()
    else()
        message(WARNING "Code coverage: REQUESTED but only works in Debug builds (current: ${CMAKE_BUILD_TYPE})")
    endif()
else()
    message(STATUS "Code coverage: DISABLED (use -DENABLE_COVERAGE=ON to enable)")
endif()

#------------------------------------------------------------------------------

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH
        "Install path prefix, prepended onto install directories." FORCE )
endif()

add_subdirectory(thirdparty)
include(cmake/sanitizers.cmake)
add_subdirectory(src)

if (NOT LOGVIEWER_SANITIZER STREQUAL "None")
    target_enable_sanitizer(LogViewer)
endif()

add_subdirectory(packaging)
add_subdirectory(tests)

enable_testing()

message(STATUS "")
message(STATUS "    == Final overview for ${PROJECT_NAME} ==")
message(STATUS "Version:              ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} ${VERSION_TYPE} @ ${VERSION_HOST}")
message(STATUS "Install prefix:       ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Compiler:             ${CMAKE_CXX_COMPILER}")
message(STATUS "CMAKE_BUILD_TYPE:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  possible options: Debug Release RelWithDebInfo MinSizeRel")
message(STATUS "")

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
  add_custom_target(doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen"
    VERBATIM)
endif()

if(APPLE AND CMAKE_CXX_COMPILER MATCHES "clang-cl$")
    message(FATAL_ERROR "clang-cl detected on macOS; use /usr/bin/clang++. Clean build/ and reconfigure.")
endif()

if(WIN32)
  # Windows-specific settings
  if(MSYS OR MINGW)
    message(STATUS "Building with MSYS2/MinGW")
  endif()
elseif(APPLE)
  # macOS-specific settings
  message(STATUS "Building on macOS")
elseif(UNIX)
  # Linux-specific settings
  message(STATUS "Building on Linux")
endif()
