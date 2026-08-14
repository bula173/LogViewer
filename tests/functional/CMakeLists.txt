cmake_minimum_required(VERSION 3.25)
project(LogViewerFunctionalTests)

# enable_testing() already called by tests/CMakeLists.txt's include(CTest)

# Functional Test Executable
add_executable(functional_tests
    SearchEngineFunctionalTest.cpp
    FilterWorkflowTest.cpp
    LargeFileHandlingTest.cpp
)

target_link_libraries(functional_tests PRIVATE
    application_core
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(functional_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# Register tests
gtest_discover_tests(functional_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add to CTest
add_test(NAME FunctionalTests COMMAND functional_tests)
