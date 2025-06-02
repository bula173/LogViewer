message(STATUS "Building LogViewer::thirdparty")
message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# Enable FetchContent or Git submodule builds
include(FetchContent)

#add_subdirectory(gflags)
add_subdirectory(gtest)
add_subdirectory(nlohmann)
add_subdirectory(spdlog)
add_subdirectory(expat)
