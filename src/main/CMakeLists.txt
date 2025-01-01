
message(STATUS "Building LogViewer::main")
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

if(APPLE)
    # create bundle on apple compiles
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${CMAKE_CURRENT_SOURCE_DIR}/MyApp.cpp)
    set(ADDITIONAL_LINKER_FLAGS "-framework Metal -framework MetalPerformanceShaders")
 
    # Set a custom plist file for the app bundle - needed for Mac OS Retina display
    set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
else()
    # the WIN32 is needed for Windows in order for it to look for WinMain
    # instead of the main function. This is ignored on other systems,
    # so it works on all platforms
    add_executable(${PROJECT_NAME} WIN32 ${CMAKE_CURRENT_SOURCE_DIR}/MyApp.cpp ${CMAKE_CURRENT_SOURCE_DIR}/main.exe.manifest)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../wxwidgets.cmake)

# Link wxWidgets
target_link_libraries(${PROJECT_NAME} PRIVATE application)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/application)

add_dependencies(${PROJECT_NAME} application)
