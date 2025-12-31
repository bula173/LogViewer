cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(LogViewer VERSION 0.1.3 DESCRIPTION "A simple log viewer" LANGUAGES CXX C)

cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# On MinGW, linking libstdc++ / libgcc statically can cause ABI/library
# conflicts depending on the toolchain. Make static linking opt-in so the
# default build uses the shared runtime which avoids duplicate-section linker
# errors on mixed environments.
if(MINGW)
    option(USE_STATIC_MINGW_LIBS "Link libstdc++ and libgcc statically on MinGW" OFF)
    if(USE_STATIC_MINGW_LIBS)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libstdc++ -static-libgcc")
    endif()
endif()

# Option to build SDK for plugin development
option(BUILD_SDK "Build SDK for plugin development" ON)

add_subdirectory(thirdparty)
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(etc)
add_subdirectory(plugins)
add_subdirectory(packaging)
