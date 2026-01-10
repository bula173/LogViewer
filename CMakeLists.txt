cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(LogViewer VERSION 1.0.1 DESCRIPTION "A modern log viewer with AI-assisted analysis" LANGUAGES CXX C)

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

# Detect if LogViewer is being used as a subproject (e.g., via FetchContent)
# In subproject mode, only build the SDK; skip full application/plugin builds
if(NOT CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    message(STATUS "LogViewer: Building as subproject (SDK only)")
    set(LOGVIEWER_SUBPROJECT TRUE)
else()
    message(STATUS "LogViewer: Building as standalone project")
    set(LOGVIEWER_SUBPROJECT FALSE)
endif()

add_subdirectory(thirdparty)
add_subdirectory(src)

if(NOT LOGVIEWER_SUBPROJECT)
    add_subdirectory(tests)
    add_subdirectory(etc)
    add_subdirectory(plugins)
    add_subdirectory(examples)
    add_subdirectory(packaging)
endif()
