cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

project(LogViewer VERSION 0.0.1
      DESCRIPTION "A simple log viewer" 
      LANGUAGES CXX)

cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

message(STATUS "Building LogViewer")
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# Enable FetchContent or Git submodule builds
include(FetchContent)

add_subdirectory(thirdparty)

add_subdirectory(src)

add_subdirectory(tests)

enable_testing()

