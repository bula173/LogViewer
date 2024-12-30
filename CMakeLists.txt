cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(LogViewer VERSION 0.0.1
      DESCRIPTION "A simple log viewer" 
      LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

add_subdirectory(thirdparty)
add_subdirectory(src)
add_subdirectory(tests)

