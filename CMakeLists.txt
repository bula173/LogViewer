cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(LogViewer VERSION 0.0.5 DESCRIPTION "A simple log viewer" LANGUAGES CXX)

cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Be nice and export compile commands by default, this is handy for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(useGoldLinker)
option(MEASURE_ALL "When enabled all commands will be passed through time command" OFF)
if(MEASURE_ALL)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "time")
endif()

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
