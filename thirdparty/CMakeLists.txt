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

# llama.cpp for local Gemma 2B inference (OPTIONAL)
# The master branch has compilation issues; full integration pending
# For now, architecture is in place for future llama.cpp integration
# Users can manually enable by:
#   1. Building llama.cpp separately
#   2. Setting ENABLE_GEMMA_INFERENCE=ON
#   3. Pointing CMAKE_PREFIX_PATH to llama.cpp install directory

option(ENABLE_GEMMA_INFERENCE "Enable local Gemma 2B inference via llama.cpp" OFF)

if(ENABLE_GEMMA_INFERENCE)
    message(STATUS "Gemma inference enabled - requires llama.cpp")
    # When ready, uncomment:
    # FetchContent_Declare(llama_cpp ...)
    # FetchContent_MakeAvailable(llama_cpp)
endif()

if(NOT LOGVIEWER_SUBPROJECT)
    add_subdirectory(gtest)
endif()
