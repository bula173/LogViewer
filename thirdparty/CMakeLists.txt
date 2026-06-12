message(STATUS "Building LogViewer::thirdparty")
message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# Enable FetchContent or Git submodule builds
include(FetchContent)

# Note: To speed up builds, dependencies are cached in build/_deps
# They only need to be downloaded once per build directory

add_subdirectory(spdlog)
add_subdirectory(expat)
add_subdirectory(fmt)
add_subdirectory(nlohmann)

# llama.cpp for embedded Gemma 2B inference
# Using stable release (b2590) with known working API
FetchContent_Declare(llama_cpp
    GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
    GIT_TAG b2590  # Stable release - avoids master branch issues
    EXCLUDE_FROM_ALL
)

# Configure llama.cpp build options
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(llama_cpp)

if(NOT LOGVIEWER_SUBPROJECT)
    add_subdirectory(gtest)
endif()
