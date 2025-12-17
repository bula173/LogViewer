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

# Distribution directories (only used for install/package phase)
set(DIST_ROOT "${PROJECT_SOURCE_DIR}/dist")
set(DIST_STAGING "${DIST_ROOT}/staging")
set(DIST_PACKAGES "${DIST_ROOT}/packages")

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

# Copy config and resources to build directory for development
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/etc)
configure_file(
    ${CMAKE_SOURCE_DIR}/etc/default_config.json
    ${CMAKE_BINARY_DIR}/bin/etc/config.json
    COPYONLY
)

# Copy field dictionary for development
configure_file(
    ${CMAKE_SOURCE_DIR}/etc/field_dictionary.json
    ${CMAKE_BINARY_DIR}/bin/etc/field_dictionary.json
    COPYONLY
)

# Run and debug targets from cmake/cleanCpppExtensions.cmake
addRunAndDebugTargets(${GUI_TARGET_NAME})

#-------------------------------------------------------------------------------
# POST_BUILD: Copy runtime dependencies to build directory for development

if(WIN32)
    # Deploy Qt runtime to build directory for development
    if(USE_QT)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt-qt6 HINTS ${Qt6_DIR}/../../../bin)
        
        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ${GUI_TARGET_NAME} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                        --verbose 1
                        --no-compiler-runtime
                        --no-translations
                        "$<TARGET_FILE:${GUI_TARGET_NAME}>"
                COMMENT "Deploying Qt runtime to build directory"
            )
        else()
            message(WARNING "windeployqt not found. Qt DLLs must be copied manually.")
        endif()
    endif()
    
    # Copy MinGW/MSYS2 runtime DLLs to build directory for development
    if(MINGW)
        add_custom_command(TARGET ${GUI_TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DARGV0="$<TARGET_FILE:${GUI_TARGET_NAME}>"
                -DARGV1="$<TARGET_FILE_DIR:${GUI_TARGET_NAME}>"
                -DARGV2="${CMAKE_C_COMPILER}"
                -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
            COMMENT "Copying MinGW runtime DLLs to build directory"
        )
    endif()
endif()

#-------------------------------------------------------------------------------
# Installation rules (for CPack)

# Install executable to the appropriate component
if(APPLE)
    # For macOS bundles, install the entire .app bundle
    install(TARGETS ${GUI_TARGET_NAME}
        BUNDLE DESTINATION "."
        COMPONENT ${CPACK_COMPONENT})
elseif(WIN32)
    # For Windows, install the executable and also copy from bin/ if needed
    install(TARGETS ${GUI_TARGET_NAME}
        RUNTIME DESTINATION "."
        COMPONENT ${CPACK_COMPONENT})
    # Extra: ensure exe is present in CPack staging root if built in bin/
    install(CODE [[
        file(GLOB BIN_EXE "${CMAKE_INSTALL_PREFIX}/../bin/${GUI_TARGET_NAME}.exe")
        if(EXISTS BIN_EXE)
            file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}" TYPE FILE FILES "${CMAKE_INSTALL_PREFIX}/../bin/${GUI_TARGET_NAME}.exe")
        endif()
    ]] COMPONENT ${CPACK_COMPONENT})
else()
    # For Linux, install the executable
    install(TARGETS ${GUI_TARGET_NAME}
        RUNTIME DESTINATION "."
        COMPONENT ${CPACK_COMPONENT})
endif()

# Install config file to common component
install(FILES ${CMAKE_SOURCE_DIR}/etc/default_config.json
    DESTINATION "etc"
    RENAME config.json
    COMPONENT common)

# Install field dictionary file to common component
install(FILES ${CMAKE_SOURCE_DIR}/etc/field_dictionary.json
    DESTINATION etc
    COMPONENT common)

# Install predefined AI prompts to common component
install(DIRECTORY ${CMAKE_SOURCE_DIR}/etc/prompts
    DESTINATION etc
    COMPONENT common
    FILES_MATCHING 
    PATTERN "*.txt"
    PATTERN "*.prompt"
    PATTERN "*.md")

# Install example log files to common component
install(FILES 
    ${CMAKE_SOURCE_DIR}/etc/example_log.xml
    ${CMAKE_SOURCE_DIR}/etc/example_log_10k.xml
    ${CMAKE_SOURCE_DIR}/etc/generate_log.py
    DESTINATION etc/examples
    COMPONENT common)

# Platform-specific runtime deployment during install
if(WIN32)
    if(USE_QT)
        # Deploy Qt runtime DLLs during install
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt-qt6 HINTS ${Qt6_DIR}/../../../bin)
        if(WINDEPLOYQT_EXECUTABLE)
            install(CODE "
                message(STATUS \"Deploying Qt runtime to install directory...\")
                execute_process(
                    COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                            --verbose 1
                            --no-compiler-runtime
                            --no-translations
                            \"\${CMAKE_INSTALL_PREFIX}/${GUI_TARGET_NAME}.exe\"
                    RESULT_VARIABLE RESULT
                )
                if(NOT RESULT EQUAL 0)
                    message(WARNING \"windeployqt failed with code \${RESULT}\")
                endif()
            " COMPONENT ${CPACK_COMPONENT})
        endif()
    endif()
    
    # Install MinGW runtime DLLs
    if(MINGW)
        install(CODE "
            message(STATUS \"Copying MinGW runtime DLLs to install directory...\")
            execute_process(
                COMMAND ${CMAKE_COMMAND}
                    -DARGV0=\"\${CMAKE_INSTALL_PREFIX}/${GUI_TARGET_NAME}.exe\"
                    -DARGV1=\"\${CMAKE_INSTALL_PREFIX}\"
                    -DARGV2=\"${CMAKE_C_COMPILER}\"
                    -P \"${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake\"
            )
        " COMPONENT ${CPACK_COMPONENT})
    endif()
elseif(APPLE)
    if(USE_QT)
        # Deploy Qt frameworks into app bundle
        find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS ${Qt6_DIR}/../../../bin)
        if(MACDEPLOYQT_EXECUTABLE)
            # Get Qt installation path for macdeployqt
            get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
            get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
            get_filename_component(QT_INSTALL_PREFIX "${QT_BIN_DIR}" DIRECTORY)
            
            install(CODE "
                set(APP_BUNDLE \"\${CMAKE_INSTALL_PREFIX}/${GUI_TARGET_NAME}.app\")
                if(EXISTS \"\${APP_BUNDLE}\")
                    message(STATUS \"Deploying Qt frameworks to app bundle: \${APP_BUNDLE}\")
                    execute_process(
                        COMMAND \"${MACDEPLOYQT_EXECUTABLE}\"
                                \"\${APP_BUNDLE}\"
                                -verbose=1
                                -qmldir=\"${CMAKE_SOURCE_DIR}\"
                        RESULT_VARIABLE RESULT
                        OUTPUT_VARIABLE OUTPUT
                        ERROR_VARIABLE OUTPUT
                    )
                    if(NOT RESULT EQUAL 0)
                        message(STATUS \"macdeployqt output: \${OUTPUT}\")
                        message(WARNING \"macdeployqt failed with code \${RESULT} - DMG may not work standalone\")
                    else()
                        message(STATUS \"Qt frameworks deployed successfully\")
                    endif()
                else()
                    message(WARNING \"App bundle not found at \${APP_BUNDLE}, skipping macdeployqt\")
                endif()
            " COMPONENT ${CPACK_COMPONENT})
        else()
            message(WARNING "macdeployqt not found - DMG will require Qt to be installed")
        endif()
    endif()
endif()

# Apply sanitizer if requested (safe now that target exists)
if(NOT LOGVIEWER_SANITIZER STREQUAL "None")
    if(TARGET ${GUI_TARGET_NAME})
        # Skip ASan on MinGW (not supported)
        if(NOT MINGW OR NOT LOGVIEWER_SANITIZER STREQUAL "Address")
            target_enable_sanitizer(${GUI_TARGET_NAME})
            message(STATUS "Sanitizer '${LOGVIEWER_SANITIZER}' enabled for target ${GUI_TARGET_NAME}")
        else()
            message(WARNING "AddressSanitizer not supported on MinGW, skipping")
        endif()
    else()
        message(WARNING "Sanitizer requested but target '${GUI_TARGET_NAME}' not found")
    endif()
endif()

