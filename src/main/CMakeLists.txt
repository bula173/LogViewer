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

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "QT_VERSION_MAJOR: ${QT_VERSION_MAJOR}")
message(STATUS "GUI_TARGET_NAME: ${GUI_TARGET_NAME}")
message(STATUS "CPACK_COMPONENT: ${CPACK_COMPONENT}")

# Distribution directories
set(DIST_ROOT "${PROJECT_SOURCE_DIR}/dist")
set(DIST_RELEASE "${DIST_ROOT}/Release")
set(DIST_LIB "${DIST_ROOT}/lib")
set(DIST_PACKAGE "${DIST_ROOT}/package")

#-------------------------------------------------------------------------------
# Version file configuration

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.cpp"
)

# Force recompilation of version.cpp to update __DATE__ macro
add_custom_target(versionFileTouchForRebuild
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")

#-------------------------------------------------------------------------------
# Icon resources

set(ICON_RESOURCE "")
if(WIN32)
    set(ICON_RESOURCE "${PROJECT_BINARY_DIR}/packaging/LogViewer.rc")
elseif(APPLE)
    set(ICON_RESOURCE "${PROJECT_SOURCE_DIR}/packaging/LogViewer.icns")
endif()

#-------------------------------------------------------------------------------
# Application executable

if(APPLE)
    add_executable(${GUI_TARGET_NAME} MACOSX_BUNDLE
        ${GUI_SOURCE_FILE}
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
        ${ICON_RESOURCE})

    set_target_properties(${GUI_TARGET_NAME} PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME ${GUI_TARGET_NAME}
        MACOSX_BUNDLE_BUNDLE_GUI_IDENTIFIER "com.LogViewer.${GUI_TARGET_NAME}"
        MACOSX_BUNDLE_ICON_FILE LogViewer.icns
        MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/packaging/MacOSXBundleInfo.plist.in)

    set_source_files_properties(${PROJECT_SOURCE_DIR}/packaging/LogViewer.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
else()
    add_executable(${GUI_TARGET_NAME} WIN32
        ${GUI_SOURCE_FILE}
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
        ${ICON_RESOURCE})
endif()

# Link UI layer
target_link_libraries(${GUI_TARGET_NAME} PRIVATE ${GUI_LIBRARY})
target_include_directories(${GUI_TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(${GUI_TARGET_NAME} versionFileTouchForRebuild ${GUI_LIBRARY})

# Copy config file to build directory for development
configure_file(
    ${CMAKE_SOURCE_DIR}/etc/default_config.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.json
    COPYONLY
)

# Run and debug targets from cmake/cleanCpppExtensions.cmake
addRunAndDebugTargets(${GUI_TARGET_NAME})

#-------------------------------------------------------------------------------
# POST_BUILD: Collect all runtime dependencies to dist/

if(WIN32)
    # Step 1: Copy executable and config to dist/Release
    add_custom_command(TARGET ${GUI_TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_RELEASE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_LIB}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${GUI_TARGET_NAME}>"
                "${DIST_RELEASE}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/etc/default_config.json"
                "${DIST_RELEASE}/config.json"
        COMMENT "Copying executable to dist/Release"
    )
    
    # Step 2: Qt-specific deployment
    if(USE_QT)
        set(QT_DEPLOY_DIR "${DIST_ROOT}/qt_deploy")
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt-qt6 HINTS ${Qt6_DIR}/../../../bin)
        
        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ${GUI_TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${QT_DEPLOY_DIR}"
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                        --verbose 1
                        --no-compiler-runtime
                        --no-translations
                        --dir "${QT_DEPLOY_DIR}"
                        "${DIST_RELEASE}/${GUI_TARGET_NAME}.exe"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${QT_DEPLOY_DIR}"
                        "${DIST_RELEASE}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${QT_DEPLOY_DIR}"
                        "$<TARGET_FILE_DIR:${GUI_TARGET_NAME}>"
                COMMENT "Deploying Qt runtime to dist/"
            )
        else()
            message(WARNING "windeployqt not found. Qt DLLs must be copied manually.")
        endif()
    endif()
    
    # Step 3: Collect MinGW/MSYS2 runtime DLLs
    if(MINGW)
        # Copy to dist/lib for packaging
        add_custom_command(TARGET ${GUI_TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DARGV0="${DIST_RELEASE}/${GUI_TARGET_NAME}.exe"
                -DARGV1="${DIST_LIB}"
                -DARGV2="${CMAKE_C_COMPILER}"
                -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
            COMMENT "Collecting MinGW runtime DLLs to dist/lib"
        )
        
        # Copy to dist/Release for complete package
        add_custom_command(TARGET ${GUI_TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DARGV0="${DIST_RELEASE}/${GUI_TARGET_NAME}.exe"
                -DARGV1="${DIST_RELEASE}"
                -DARGV2="${CMAKE_C_COMPILER}"
                -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
            COMMENT "Copying MinGW runtime DLLs to dist/Release"
        )
        
        # Copy to build dir for local testing
        add_custom_command(TARGET ${GUI_TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DARGV0="$<TARGET_FILE:${GUI_TARGET_NAME}>"
                -DARGV1="$<TARGET_FILE_DIR:${GUI_TARGET_NAME}>"
                -DARGV2="${CMAKE_C_COMPILER}"
                -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
            COMMENT "Copying runtime DLLs to build directory for testing"
        )
    endif()
endif()

#-------------------------------------------------------------------------------
# Installation rules (for CPack)

# Install executable
install(TARGETS ${GUI_TARGET_NAME}
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "."
    COMPONENT ${CPACK_COMPONENT})

# Install config file
install(FILES ${CMAKE_SOURCE_DIR}/etc/default_config.json
    DESTINATION "."
    RENAME config.json
    COMPONENT common)

if(USE_QT AND EXISTS "${DIST_ROOT}/qt_deploy")
    # Install Qt runtime files
    install(DIRECTORY "${DIST_ROOT}/qt_deploy/"
        DESTINATION "."
        COMPONENT ${CPACK_COMPONENT})
endif()

if(EXISTS "${DIST_LIB}")
    # Install all collected DLLs from dist/lib
    install(DIRECTORY "${DIST_LIB}/"
        DESTINATION "."
        COMPONENT ${CPACK_COMPONENT}
        FILES_MATCHING PATTERN "*.dll")
endif()

