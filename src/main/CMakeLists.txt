if (WIN32)
    configure_file(
        ${PROJECT_SOURCE_DIR}/packaging/LogViewer.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.rc
    )
    configure_file(
        ${PROJECT_SOURCE_DIR}/packaging/LogViewer.manifest
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.manifest
        COPYONLY
    )
    configure_file(
        ${PROJECT_SOURCE_DIR}/packaging/LogViewer.ico
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.ico
        COPYONLY
    )
endif()

# ============================================================
# Executable (only build when not used as subproject)
# ============================================================

if(NOT LOGVIEWER_SUBPROJECT)

add_executable(${PROJECT_NAME})

# Correct the path to include the modular CMake files
# include(../cmake/EnableSanitizers.cmake)
# include(../cmake/Cppcheck.cmake)
# include(../cmake/ClangTidy.cmake)

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if (APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)

    # Avoid embedding Homebrew Qt paths into the installed bundle.
    # We want the app to resolve frameworks from inside the bundle.
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH_USE_LINK_PATH FALSE
    )
endif()

qt_add_resources(RESOURCE_FILES
    ${PROJECT_SOURCE_DIR}/resources/styles.qrc
)

target_sources(${PROJECT_NAME}
    PRIVATE
        MyAppQt.cpp
        ${RESOURCE_FILES}
)

if (WIN32)
    target_sources(${PROJECT_NAME}
        PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.rc
    )
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        application_core
)

endif()

# ============================================================
# Install main executable (only when built standalone, not as subproject)
# ============================================================

if(NOT LOGVIEWER_SUBPROJECT)
    if (APPLE)
        install(
            DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.app
            DESTINATION .
            USE_SOURCE_PERMISSIONS
            COMPONENT application
        )
    else()
        install(
            TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION . 
            COMPONENT application
        )
    endif()
endif()

# ============================================================
# macOS deployment
# ============================================================

if (APPLE AND NOT LOGVIEWER_SUBPROJECT)
    set(LOGVIEWER_CODESIGN_IDENTITY "-" CACHE STRING "codesign identity to use for macOS app bundle signing (default: ad-hoc '-')")

    configure_file(
        ${CMAKE_SOURCE_DIR}/packaging/MacOSXBundleInfo.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    )

    find_program(
        MACDEPLOYQT_EXECUTABLE
        macdeployqt
        HINTS /opt/homebrew/opt/qt/bin
    )

    if (MACDEPLOYQT_EXECUTABLE)
        add_custom_command(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${MACDEPLOYQT_EXECUTABLE}
                    $<TARGET_BUNDLE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${CMAKE_SOURCE_DIR}/etc/qt.conf
                    $<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/qt.conf
            # macdeployqt (and/or install_name_tool) can invalidate signatures of copied dylibs.
            # Re-sign the bundle (default: ad-hoc) so macOS doesn't SIGKILL on cs_invalid_page.
            COMMAND /usr/bin/codesign
                --force
                --deep
                    --sign "${LOGVIEWER_CODESIGN_IDENTITY}"
                $<TARGET_BUNDLE_DIR:${PROJECT_NAME}>
            COMMENT "Running macdeployqt + codesign"
        )
    endif()
endif()

if (WIN32)
    if(NOT LOGVIEWER_SUBPROJECT)
        # Deploy Qt at install time
        find_program(
            WINDEPLOYQT_EXECUTABLE
            windeployqt
            HINTS ${Qt6_DIR}/../../../bin
            REQUIRED
        )

        install(CODE "
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                        --no-translations
                        --no-system-d3d-compiler
                        --compiler-runtime
                        \"\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.exe\"  # Updated for root
                        
            )
        "
        COMPONENT application
        )

        install(CODE "
            get_filename_component(COMPILER_DIR \"${CMAKE_CXX_COMPILER}\" DIRECTORY)
            message(STATUS \"Analyzing dependencies for: $<TARGET_FILE:${PROJECT_NAME}>\")
            message(STATUS \"Toolchain bin: \$\{COMPILER_DIR\}\")

            file(GET_RUNTIME_DEPENDENCIES
                EXECUTABLES \"$<TARGET_FILE:${PROJECT_NAME}>\"
                RESOLVED_DEPENDENCIES_VAR _resolved_deps
                UNRESOLVED_DEPENDENCIES_VAR _unresolved_deps
                DIRECTORIES \"\$\{COMPILER_DIR\}\"
                PRE_EXCLUDE_REGEXES \"api-ms-.*\" \"ext-ms-.*\"
                POST_EXCLUDE_REGEXES \".*[Ss]ystem32/.*\\.dll\" \".*[Ww]indows/.*\\.dll\"  \"Qt6.*\.dll\"
            )

            set(COPIED_COUNT 0)
            foreach(DEP \${_resolved_deps})
                # Only copy DLLs from the toolchain directory (skip Windows system DLLs)
                string(FIND \"\${DEP}\" \"mingw64\" TOOLCHAIN_MATCH)
                if(NOT TOOLCHAIN_MATCH EQUAL -1)
                    get_filename_component(DLL_NAME \"\${DEP}\" NAME)
                    
                    file(INSTALL
                            DESTINATION \"\${CMAKE_INSTALL_PREFIX}\"
                            TYPE SHARED_LIBRARY
                            FILES \"\${DEP}\"
                        )
                        message(STATUS \"Copied: \${DLL_NAME}\")
                        math(EXPR COPIED_COUNT \"\${COPIED_COUNT} + 1\")
                endif()
            endforeach()

            # Print warning if any non-excluded DLL was not found
            foreach(_file IN LISTS \${_unresolved_deps})
                if(_file STREQUAL \"\")
                    continue()
                endif()
                message(WARNING \"Dependency not found: \${_file}\")
            endforeach()

            # Debugging: Print resolved and unresolved dependencies
            message(STATUS \"Resolved dependencies: \${_resolved_deps}\")
            message(STATUS \"Unresolved dependencies: \${_unresolved_deps}\")
        " COMPONENT application)
    endif()
endif()
# ============================================================
# Linux (optional)
# ============================================================

if (UNIX AND NOT APPLE AND NOT LOGVIEWER_SUBPROJECT)
    find_program(LINUXDEPLOYQT_EXECUTABLE linuxdeployqt HINTS /usr/bin)

    if (LINUXDEPLOYQT_EXECUTABLE)
        add_custom_command(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${LINUXDEPLOYQT_EXECUTABLE}
                    $<TARGET_FILE:${PROJECT_NAME}>
                    -deploy-deps-only
            COMMENT "Running linuxdeployqt"
        )
    endif()
endif()


