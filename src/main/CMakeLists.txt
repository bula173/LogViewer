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
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DIST_BUILD "${DIST_ROOT}/Debug")
else()
    set(DIST_BUILD "${DIST_ROOT}/Release")
endif()
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
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/etc/config.json
    COPYONLY
)

# Run and debug targets from cmake/cleanCpppExtensions.cmake
addRunAndDebugTargets(${GUI_TARGET_NAME})

#-------------------------------------------------------------------------------
# POST_BUILD: Collect all runtime dependencies to dist/

if(WIN32)
    # Step 1: Copy executable and config to dist/Debug or dist/Release
    add_custom_command(TARGET ${GUI_TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_BUILD}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_LIB}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${GUI_TARGET_NAME}>"
                "${DIST_BUILD}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/etc/default_config.json"
                "${DIST_BUILD}/etc/config.json"
        COMMENT "Copying executable to dist/${CMAKE_BUILD_TYPE}"
    )
    
    # Step 2: Qt-specific deployment
    if(USE_QT)
        set(QT_DEPLOY_DIR "${DIST_ROOT}/qt_deploy_${CMAKE_BUILD_TYPE}")
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt-qt6 HINTS ${Qt6_DIR}/../../../bin)
        
        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ${GUI_TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${QT_DEPLOY_DIR}"
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                        --verbose 1
                        --no-compiler-runtime
                        --no-translations
                        --dir "${QT_DEPLOY_DIR}"
                        "${DIST_BUILD}/${GUI_TARGET_NAME}.exe"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${QT_DEPLOY_DIR}"
                        "${DIST_BUILD}"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${QT_DEPLOY_DIR}"
                        "$<TARGET_FILE_DIR:${GUI_TARGET_NAME}>"
                COMMENT "Deploying Qt runtime to dist/${CMAKE_BUILD_TYPE}"
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
                -DARGV0="${DIST_BUILD}/${GUI_TARGET_NAME}.exe"
                -DARGV1="${DIST_LIB}"
                -DARGV2="${CMAKE_C_COMPILER}"
                -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
            COMMENT "Collecting MinGW runtime DLLs to dist/lib"
        )
        
        # Copy to dist/Debug or dist/Release for complete package
        add_custom_command(TARGET ${GUI_TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DARGV0="${DIST_BUILD}/${GUI_TARGET_NAME}.exe"
                -DARGV1="${DIST_BUILD}"
                -DARGV2="${CMAKE_C_COMPILER}"
                -P ${CMAKE_SOURCE_DIR}/cmake/copy_dlls.cmake
            COMMENT "Copying MinGW runtime DLLs to dist/${CMAKE_BUILD_TYPE}"
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

# Install executable to the appropriate component
install(TARGETS ${GUI_TARGET_NAME}
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "."
    COMPONENT ${CPACK_COMPONENT})  # This is "qt_app" for Qt builds

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

if(USE_QT)
    # Install all Qt runtime files (DLLs, plugins, etc.) to qt_app component
    install(CODE "
        set(DIST_BUILD \"${DIST_BUILD}\")
        set(BUILD_TYPE \"${CMAKE_BUILD_TYPE}\")
        set(COMPONENT_NAME \"${CPACK_COMPONENT}\")
        
        message(STATUS \"=== Installing Qt application (component: \${COMPONENT_NAME}) ===\")
        message(STATUS \"Build Type: \${BUILD_TYPE}\")
        message(STATUS \"Source: \${DIST_BUILD}\")
        message(STATUS \"Destination: \${CMAKE_INSTALL_PREFIX}\")
        
        if(EXISTS \"\${DIST_BUILD}\")
            # Get all files recursively from dist/Debug or dist/Release
            file(GLOB_RECURSE ALL_FILES RELATIVE \"\${DIST_BUILD}\" \"\${DIST_BUILD}/*\")
            
            set(FILE_COUNT 0)
            foreach(FILE \${ALL_FILES})
                # Skip the executable, config.json, and plugin libraries (they're installed separately)
                # Skip .exe files, plugin DLLs, plugin .so/.dylib, and config.json
                if(NOT FILE MATCHES \"\\\\.exe$\" 
                   AND NOT FILE STREQUAL \"config.json\"
                   AND NOT FILE MATCHES \"plugins/.*\\\\.(dll|so|dylib)$\"
                   AND NOT FILE MATCHES \"lib/plugins/.*\\\\.(dll|so|dylib)$\")
                    get_filename_component(FILE_DIR \"\${FILE}\" DIRECTORY)
                    
                    # Create destination directory if needed
                    if(FILE_DIR)
                        file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/\${FILE_DIR}\")
                    endif()
                    
                    # Install the file
                    file(INSTALL \"\${DIST_BUILD}/\${FILE}\" 
                         DESTINATION \"\${CMAKE_INSTALL_PREFIX}/\${FILE_DIR}\")
                    
                    math(EXPR FILE_COUNT \"\${FILE_COUNT} + 1\")
                endif()
            endforeach()
            
            message(STATUS \"Installed \${FILE_COUNT} Qt runtime files\")
            message(STATUS \"=== Installation complete ===\")
        else()
            message(FATAL_ERROR \"Qt build directory not found: \${DIST_BUILD} (Build type: \${BUILD_TYPE})\")
        endif()
    " COMPONENT ${CPACK_COMPONENT})
    
else()
    # For non-Qt builds (wxWidgets): Install DLLs from dist/lib
    install(CODE "
        set(DIST_LIB \"${DIST_LIB}\")
        set(COMPONENT_NAME \"${CPACK_COMPONENT}\")
        
        message(STATUS \"=== Installing runtime DLLs (component: \${COMPONENT_NAME}) ===\")
        
        if(EXISTS \"\${DIST_LIB}\")
            file(GLOB RUNTIME_DLLS \"\${DIST_LIB}/*.dll\")
            
            set(DLL_COUNT 0)
            foreach(DLL \${RUNTIME_DLLS})
                file(INSTALL \"\${DLL}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")
                math(EXPR DLL_COUNT \"\${DLL_COUNT} + 1\")
            endforeach()
            
            message(STATUS \"Installed \${DLL_COUNT} runtime DLLs\")
        else()
            message(WARNING \"Runtime DLL directory not found: \${DIST_LIB}\")
        endif()
    " COMPONENT ${CPACK_COMPONENT})
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

