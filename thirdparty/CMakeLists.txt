message(STATUS "Building LogViewer::thirdparty")
message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# Enable FetchContent or Git submodule builds
include(FetchContent)

# Note: To speed up builds, dependencies are cached in build/_deps
# They only need to be downloaded once per build directory

if(NOT LOGVIEWER_SUBPROJECT)
    add_subdirectory(gtest)
    add_subdirectory(spdlog)
    add_subdirectory(expat)
    add_subdirectory(fmt)
    add_subdirectory(nlohmann)
endif()
