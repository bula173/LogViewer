if (WIN32)
    message(STATUS "Tests are disabled on Windows")
    return()
endif()

# Updated runtime output directory to align with default build directory structure
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin)

message(STATUS "Building LogViewer::tests")

file(GLOB TEST_SOURCES "*.cpp")

add_executable(
  ${PROJECT_NAME}_tests
  ${TEST_SOURCES}
  ${CMAKE_BINARY_DIR}/src/main/version.cpp
)

configure_file(
    ${CMAKE_SOURCE_DIR}/etc/default_config.json
    ${CMAKE_BINARY_DIR}/tests/config.json
    COPYONLY
)

target_compile_definitions(${PROJECT_NAME}_tests PRIVATE TESTING)

target_include_directories(${PROJECT_NAME}_tests PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME}_tests
  gtest_main
  gmock
  application_core
)

include(GoogleTest)
include(CTest)

if (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Skipping test discovery in Debug on Windows")
else()
    gtest_discover_tests(${PROJECT_NAME}_tests)
endif()

# Enable testing
enable_testing()