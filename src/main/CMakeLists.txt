message(STATUS "Building LogViewer::main with ${GUI_FRAMEWORK}")


# Select the appropriate source file and target name based on GUI framework
if(USE_WXWIDGETS)
    set(GUI_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/MyApp.cpp")
    set(GUI_TARGET_NAME "${PROJECT_NAME}Wx")
    set(GUI_LIBRARY "application_wx")
    set(CPACK_COMPONENT "wx_app")
elseif(USE_QT)
    set(GUI_SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/MyAppQt.cpp")
    set(GUI_TARGET_NAME "${PROJECT_NAME}Qt")
    set(GUI_LIBRARY "application_qt")
    set(CPACK_COMPONENT "qt_app")
else()
    message(FATAL_ERROR "No GUI framework selected!")
endif()

message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})
message(STATUS "QT_VERSION_MAJOR=${QT_VERSION_MAJOR}")
message(STATUS "GUI_TARGET_NAME=${GUI_TARGET_NAME}")
message(STATUS "CPACK_COMPONENT=${CPACK_COMPONENT}")

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
# Application executable setup

set(ICON_RESOURCE "")
if(WIN32) #windows icon resource file, provided from /packaging directory
    set(ICON_RESOURCE "${PROJECT_BINARY_DIR}/packaging/LogViewer.rc")
elseif (APPLE) #osx icon ressource file, provided from /packaging directory
    set(ICON_RESOURCE "${PROJECT_SOURCE_DIR}/packaging/LogViewer.icns")
endif()


if(APPLE)
    # create bundles on Apple builds
    add_executable(${GUI_TARGET_NAME} MACOSX_BUNDLE
        ${GUI_SOURCE_FILE}
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
        ${ICON_RESOURCE})

    set_target_properties(${GUI_TARGET_NAME}
        PROPERTIES
            MACOSX_BUNDLE_BUNDLE_NAME ${GUI_TARGET_NAME}
            MACOSX_BUNDLE_BUNDLE_GUI_IDENTIFIER "com.LogViewer.${GUI_TARGET_NAME}"
            MACOSX_BUNDLE_ICON_FILE LogViewer.icns
            MACOSX_BUNDLE_INFO_PLIST
                ${PROJECT_SOURCE_DIR}/packaging/MacOSXBundleInfo.plist.in)

    set_source_files_properties(${PROJECT_SOURCE_DIR}/packaging/LogViewer.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
else()
    # the WIN32 is needed for Windows in order for it to look for WinMain
    # instead of the main function. This is ignored on other systems,
    # so it works on all platforms
    add_executable(${GUI_TARGET_NAME} WIN32
        ${GUI_SOURCE_FILE}
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
        ${ICON_RESOURCE})

    # Path where DLLs should be copied (e.g., next to the .exe)
    if(MINGW)
        # Copy DLLs to the executable directory (for running during development)
        add_custom_target(copy_runtime_dlls_to_exe ALL
            COMMAND bash -c "${CMAKE_COMMAND} -DARGV0='$<TARGET_FILE:${GUI_TARGET_NAME}>' -DARGV1='$<TARGET_FILE_DIR:${GUI_TARGET_NAME}>' -DARGV2=${CMAKE_C_COMPILER} -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake"
            COMMENT "Copying runtime DLLs to executable directory"
            DEPENDS ${GUI_TARGET_NAME})
        # Copy DLLs to dist/lib (for packaging)
        set(DLL_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/dist/lib")
        add_custom_target(copy_runtime_dlls_to_dist ALL
            COMMAND bash -c "${CMAKE_COMMAND} -DARGV0='$<TARGET_FILE:${GUI_TARGET_NAME}>' -DARGV1=${DLL_OUTPUT_DIR} -DARGV2=${CMAKE_C_COMPILER} -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake"
            COMMENT "Copying runtime DLLs to dist/lib for packaging"
            DEPENDS ${GUI_TARGET_NAME})
    endif()
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/etc/default_config.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.json
    COPYONLY
)


install(TARGETS ${GUI_TARGET_NAME}
        BUNDLE DESTINATION "."
        RUNTIME DESTINATION "."
        COMPONENT ${CPACK_COMPONENT})

if (USE_QT)
    #install all files deployed by windeployqt
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/dist/qt_deploy/"
            DESTINATION "."
            COMPONENT ${CPACK_COMPONENT})
endif()

install(FILES ${CMAKE_SOURCE_DIR}/etc/default_config.json
    DESTINATION "."
    COMPONENT common)




# Using macro from cmake/cleanCpppExtensions.cpp included on the top
# Provides ${PROJECT_NAME}-run and ${PROJECT_NAME}-dbg
addRunAndDebugTargets(${GUI_TARGET_NAME})

# Link UI layer
target_link_libraries(${GUI_TARGET_NAME} PRIVATE ${GUI_LIBRARY})

target_include_directories(${GUI_TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(${GUI_TARGET_NAME} versionFileTouchForRebuild ${GUI_LIBRARY})

# On Windows with Qt, explicitly run windeployqt at install time to make the
# install tree (CMAKE_INSTALL_PREFIX) self-contained.
if(WIN32 AND USE_QT)
    # Try to locate windeployqt from a typical MSYS2 Qt6 installation.
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt-qt6 windeployqt
        HINTS
            ENV QT_HOST_PATH
            C:/msys64/mingw64/bin
            C:/msys64/mingw32/bin
    )

    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(WARNING "windeployqt not found; Qt runtime deployment will be skipped.")
    else()
        message(STATUS "Using windeployqt: ${WINDEPLOYQT_EXECUTABLE}")

        # Directory in the build tree where we stage the deployed Qt runtime.
        set(QT_DEPLOY_DIR "${PROJECT_SOURCE_DIR}/dist/qt_deploy")

        add_custom_target(deploy_qt_runtime ALL
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${QT_DEPLOY_DIR}"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --verbose 1
                    --no-compiler-runtime
                    --no-quick-import
                    --no-system-d3d-compiler
                    "$<TARGET_FILE:${GUI_TARGET_NAME}>"
                    --dir "${QT_DEPLOY_DIR}"
            DEPENDS ${GUI_TARGET_NAME}
            COMMENT "Deploying Qt runtime for ${GUI_TARGET_NAME} into ${QT_DEPLOY_DIR}"

            #copy qt_deploy into executable directory
            COMMAND "${CMAKE_COMMAND}" -E copy_directory "${QT_DEPLOY_DIR}" "$<TARGET_FILE_DIR:${GUI_TARGET_NAME}>"
        )
    endif()
endif()

