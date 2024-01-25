cmake_minimum_required(VERSION 3.6 FATAL_ERROR)

project(LogViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_custom_target(clean-all
   COMMAND ${CMAKE_BUILD_TOOL} clean
   COMMAND ${CMAKE_COMMAND} -P clean-all.cmake
)

# ExternalProject base
set_property(DIRECTORY PROPERTY EP_BASE ${CMAKE_BINARY_DIR}/subprojects)

set(STAGED_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/stage)

add_subdirectory(thirdparty)
add_subdirectory(test)


include(ExternalProject)
ExternalProject_Add(${PROJECT_NAME}_core
  DEPENDS
  wxWidgets_external
  SOURCE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  CMAKE_ARGS
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}

  -DwxWidgets_ROOT_DIR=${wxWidgets_ROOT_DIR}
  -DENV_WX_CONFIG=${ENV_WX_CONFIG}
  CMAKE_CACHE_ARGS
  -DCMAKE_PREFIX_PATH:PATH=${CMAKE_PREFIX_PATH}
  BUILD_ALWAYS
  1
  INSTALL_COMMAND
  ""
)