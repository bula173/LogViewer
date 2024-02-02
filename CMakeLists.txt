cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(LogViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

add_custom_target(clean-all
   COMMAND ${CMAKE_BUILD_TOOL} clean
   COMMAND ${CMAKE_COMMAND} -P clean-all.cmake
)

add_subdirectory(thirdparty)
add_subdirectory(src)
add_subdirectory(test)

