set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/bin)

message(STATUS "Building LogViewer::tests")

file(GLOB TEST_SOURCES "*.cpp")

add_executable(
  ${PROJECT_NAME}_tests
  ${TEST_SOURCES}
)

configure_file(
    ${CMAKE_SOURCE_DIR}/etc/config.json
    ${CMAKE_BINARY_DIR}/tests/config.json
    COPYONLY
)

target_compile_definitions(${PROJECT_NAME}_tests PRIVATE TESTING)

target_include_directories(${PROJECT_NAME}_tests PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME}_tests
  gtest_main
  gmock
  application
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests)

# Enable testing
enable_testing()