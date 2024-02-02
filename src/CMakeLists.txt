
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/Debug)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/Release)
else()
    message("----Unknown build type-------")
endif()

message(STATUS "Building LogViewer....")

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.hpp)

if(APPLE)
    # create bundle on apple compiles
    add_executable(LogViewer MACOSX_BUNDLE ${SRCS})

    # Set a custom plist file for the app bundle - needed for Mac OS Retina display
    set_target_properties(LogViewer PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
else()
    # the WIN32 is needed for Windows in order for it to look for WinMain
    # instead of the main function. This is ignored on other systems,
    # so it works on all platforms
    add_executable(LogViewer WIN32 ${SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/main.exe.manifest)
endif()

target_include_directories(LogViewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${HDRS} ${wxWidgets_SOURCE_DIR}/include)
target_link_libraries(LogViewer PRIVATE wxcore wxnet)