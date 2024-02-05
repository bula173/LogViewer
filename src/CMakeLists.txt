
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/Debug)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/Release)
else()
    message("----Unknown build type-------")
endif()

message(STATUS "Building LogViewer....")

find_program(
    CLANG_TIDY_EXE
    NAMES clang-tidy
)


file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.hpp)


if(APPLE)
    # create bundle on apple compiles
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${SRCS})
    set(ADDITIONAL_LINKER_FLAGS "-framework Metal -framework MetalPerformanceShaders")

    # Set a custom plist file for the app bundle - needed for Mac OS Retina display
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
else()
    # the WIN32 is needed for Windows in order for it to look for WinMain
    # instead of the main function. This is ignored on other systems,
    # so it works on all platforms
    add_executable(${PROJECT_NAME} WIN32 ${SRCS} ${CMAKE_CURRENT_SOURCE_DIR}/main.exe.manifest)
endif()

if (CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(DO_CLANG_TIDY
        "${CLANG_TIDY_EXE}"
        "--header-filter=${CMAKE_CURRENT_SOURCE_DIR}/*"
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
else()
    message(STATUS "clang-tidy not found.")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${HDRS} ${wxWidgets_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE wxcore wxnet)