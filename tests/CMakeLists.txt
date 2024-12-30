set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

file(GLOB TEST_SOURCES "*.cpp")

add_executable(
  all_tests
  ${TEST_SOURCES}
)

target_include_directories(
  all_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${GLOG_INCLUDE_DIR}
)

find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

target_compile_definitions(all_tests PRIVATE TESTING)

target_link_libraries(
  all_tests
  gtest_main
  gmock
  ${wxWidgets_LIBRARIES}
  glog
  ${PROJECT_NAME}_lib
)

include(GoogleTest)
gtest_discover_tests(all_tests)