set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/bin)

message(STATUS "Building LogViewer::tests")

file(GLOB TEST_SOURCES "*.cpp")

add_executable(
  ${PROJECT_NAME}_tests
  ${TEST_SOURCES}
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
  application
)

include(GoogleTest)

if (WIN32)
    # Do NOT run the exe during build; just register it.
    add_test(NAME LogViewerTests COMMAND LogViewer_tests)
    # Ensure runtime can find wx/expat DLLs
    if (DEFINED wxWidgets_LIB_DIR)
        set_tests_properties(LogViewerTests PROPERTIES
            ENVIRONMENT "PATH=$<TARGET_FILE_DIR:LogViewer_tests>;$<TARGET_FILE_DIR:LogViewer_tests>/..;${wxWidgets_LIB_DIR};$ENV{PATH}")
    else()
        set_tests_properties(LogViewerTests PROPERTIES
            ENVIRONMENT "PATH=$<TARGET_FILE_DIR:LogViewer_tests>;$ENV{PATH}")
    endif()
else()
    # Normal automatic discovery on non-Windows
    gtest_discover_tests(LogViewer_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DISCOVERY_TIMEOUT 60
    )
endif()

# (Optional) Copy required DLLs next to the test exe (fallback if PATH still problematic)
if (WIN32)
    file(GLOB WX_DLLS "${wxWidgets_LIB_DIR}/*.dll")
    add_custom_command(TARGET LogViewer_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WX_DLLS} $<TARGET_FILE_DIR:LogViewer_tests>
        VERBATIM
    )
endif()

# Enable testing
enable_testing()