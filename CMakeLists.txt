cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(LogViewer VERSION 0.1.1 DESCRIPTION "A simple log viewer" LANGUAGES CXX C)

cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc -static")
endif()

add_subdirectory(thirdparty)
add_subdirectory(src/application)
add_subdirectory(src/main)
add_subdirectory(tests)
add_subdirectory(packaging)