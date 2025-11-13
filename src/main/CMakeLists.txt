message(STATUS "Building LogViewer::main")
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# configure version.cpp.in with selected version
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/version.cpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/version.cpp"
  )
# If you want to have date and time of the build, your targets has to depend on this.
# This will force recompilation of version.o and thus forcing gcc to update __DATE__ macro.
add_custom_target(versionFileTouchForRebuild
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")
#-------------------------------------------------------------------------------
# exampleApp executable setup

set(ICON_RESOURCE "")
if(WIN32) #windows icon resource file, provided from /packaging directory
    set(ICON_RESOURCE "${PROJECT_BINARY_DIR}/packaging/LogViewer.rc")
elseif (APPLE) #osx icon ressource file, provided from /packaging directory
    set(ICON_RESOURCE "${PROJECT_SOURCE_DIR}/packaging/exampleApp.icns")
endif()


if(APPLE)

    # create bundle on apple compiles
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${CMAKE_CURRENT_SOURCE_DIR}/MyApp.cpp ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)

    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_BUNDLE_GUI_IDENTIFIER "com.LogViewer.LogViewer")
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE exampleApp.icns)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/packaging/MacOSXBundleInfo.plist.in)
    set_source_files_properties(${PROJECT_SOURCE_DIR}/packaging/exampleApp.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
else()
    # the WIN32 is needed for Windows in order for it to look for WinMain
    # instead of the main function. This is ignored on other systems,
    # so it works on all platforms
    add_executable(${PROJECT_NAME} WIN32 
        ${CMAKE_CURRENT_SOURCE_DIR}/MyApp.cpp 
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp  
        ${ICON_RESOURCE})
    # Path where DLLs should be copied (e.g., next to the .exe)
    if(MINGW)
        # Copy DLLs to the executable directory (for running during development)
        add_custom_target(copy_runtime_dlls_to_exe ALL
            COMMAND ${CMAKE_COMMAND} -DARGV0="$<TARGET_FILE:${PROJECT_NAME}>" -DARGV1="$<TARGET_FILE_DIR:${PROJECT_NAME}>" -DARGV2=${CMAKE_C_COMPILER}
                    -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
            COMMENT "Copying runtime DLLs to executable directory"
            DEPENDS ${PROJECT_NAME}
        )
        # Copy DLLs to dist/lib (for packaging)
        set(DLL_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/dist/lib")
        add_custom_target(copy_runtime_dlls_to_dist ALL
            COMMAND ${CMAKE_COMMAND} -DARGV0="$<TARGET_FILE:${PROJECT_NAME}>" -DARGV1=${DLL_OUTPUT_DIR} -DARGV2=${CMAKE_C_COMPILER}
                    -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
            COMMENT "Copying runtime DLLs to dist/lib for packaging"
            DEPENDS ${PROJECT_NAME}
        )
    endif()
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/etc/default_config.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.json
    COPYONLY
)

install(TARGETS ${PROJECT_NAME} 
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION ".")

install(FILES ${CMAKE_SOURCE_DIR}/etc/default_config.json DESTINATION ".")

# Using macro from cmake/cleanCpppExtensions.cpp included on the top
# Provides ${PROJECT_NAME}-run and ${PROJECT_NAME}-dbg
addRunAndDebugTargets(${PROJECT_NAME})

# Link wxWidgets
target_link_libraries(${PROJECT_NAME} PRIVATE application)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(${PROJECT_NAME} versionFileTouchForRebuild application)

