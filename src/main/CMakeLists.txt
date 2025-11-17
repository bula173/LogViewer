message(STATUS "Building LogViewer::main")
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

set(LOGVIEWER_QT_TARGET "${PROJECT_NAME}Qt")

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

    # create bundles on Apple builds
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE
        ${CMAKE_CURRENT_SOURCE_DIR}/MyApp.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)

    add_executable(${LOGVIEWER_QT_TARGET} MACOSX_BUNDLE
        ${CMAKE_CURRENT_SOURCE_DIR}/MyAppQt.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)

    foreach(target_name IN ITEMS ${PROJECT_NAME} ${LOGVIEWER_QT_TARGET})
        set_target_properties(${target_name}
            PROPERTIES
                MACOSX_BUNDLE_BUNDLE_NAME ${target_name}
                MACOSX_BUNDLE_BUNDLE_GUI_IDENTIFIER "com.LogViewer.${target_name}"
                MACOSX_BUNDLE_ICON_FILE exampleApp.icns
                MACOSX_BUNDLE_INFO_PLIST
                    ${PROJECT_SOURCE_DIR}/packaging/MacOSXBundleInfo.plist.in)
    endforeach()
    set_source_files_properties(${PROJECT_SOURCE_DIR}/packaging/exampleApp.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
else()
    # the WIN32 is needed for Windows in order for it to look for WinMain
    # instead of the main function. This is ignored on other systems,
    # so it works on all platforms
    add_executable(${PROJECT_NAME} WIN32
        ${CMAKE_CURRENT_SOURCE_DIR}/MyApp.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
        ${ICON_RESOURCE})

    add_executable(${LOGVIEWER_QT_TARGET} WIN32
        ${CMAKE_CURRENT_SOURCE_DIR}/MyAppQt.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
        ${ICON_RESOURCE})

    # Path where DLLs should be copied (e.g., next to the .exe)
    if(MINGW)
        foreach(target_name IN ITEMS ${PROJECT_NAME} ${LOGVIEWER_QT_TARGET})
            # Copy DLLs to the executable directory (for running during development)
            add_custom_target(copy_runtime_dlls_to_exe_${target_name} ALL
                COMMAND ${CMAKE_COMMAND} -DARGV0="$<TARGET_FILE:${target_name}>" -DARGV1="$<TARGET_FILE_DIR:${target_name}>" -DARGV2=${CMAKE_C_COMPILER}
                        -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
                COMMENT "Copying runtime DLLs to executable directory (${target_name})"
                DEPENDS ${target_name})

            # Copy DLLs to dist/lib (for packaging)
            set(DLL_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/dist/lib")
            add_custom_target(copy_runtime_dlls_to_dist_${target_name} ALL
                COMMAND ${CMAKE_COMMAND} -DARGV0="$<TARGET_FILE:${target_name}>" -DARGV1=${DLL_OUTPUT_DIR} -DARGV2=${CMAKE_C_COMPILER}
                        -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
                COMMENT "Copying runtime DLLs to dist/lib for packaging (${target_name})"
                DEPENDS ${target_name})
        endforeach()
    endif()
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/etc/default_config.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.json
    COPYONLY
)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "."
    COMPONENT wx_app)

install(TARGETS ${LOGVIEWER_QT_TARGET}
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "."
    COMPONENT qt_app)

install(FILES ${CMAKE_SOURCE_DIR}/etc/default_config.json
    DESTINATION "."
    COMPONENT common)

# Install runtime DLLs on Windows
if(WIN32 AND MINGW)
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/dist/lib/
        DESTINATION "."
        FILES_MATCHING PATTERN "*.dll"
        COMPONENT common)
endif()

# Using macro from cmake/cleanCpppExtensions.cpp included on the top
# Provides ${PROJECT_NAME}-run and ${PROJECT_NAME}-dbg
addRunAndDebugTargets(${PROJECT_NAME})
addRunAndDebugTargets(${LOGVIEWER_QT_TARGET})

# Link UI layers
target_link_libraries(${PROJECT_NAME} PRIVATE application_wx)
target_link_libraries(${LOGVIEWER_QT_TARGET} PRIVATE application_qt)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${LOGVIEWER_QT_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(${PROJECT_NAME} versionFileTouchForRebuild application_wx)
add_dependencies(${LOGVIEWER_QT_TARGET} versionFileTouchForRebuild application_qt)

