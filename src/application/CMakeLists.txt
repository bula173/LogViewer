message(STATUS "Building LogViewer::application")
message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

include(${CMAKE_CURRENT_SOURCE_DIR}/../wxwidgets.cmake)

file(GLOB_RECURSE APP_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(application STATIC ${APP_SRCS})

target_include_directories(application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${wxWidgets_INCLUDE_DIRS})

# Link wxWidgets
target_link_libraries(application INTERFACE ${wxWidgets_LIBRARIES})


