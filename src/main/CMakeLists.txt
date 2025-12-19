# ============================================================
# Version / resources
# ============================================================

configure_file(
    version.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
)

if (WIN32)
    configure_file(
        ${CMAKE_SOURCE_DIR}/packaging/LogViewer.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.rc
    )
    configure_file(
        ${CMAKE_SOURCE_DIR}/packaging/LogViewer.manifest
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.manifest
        COPYONLY
    )
    configure_file(
        ${CMAKE_SOURCE_DIR}/packaging/LogViewer.ico
        ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.ico
        COPYONLY
    )
endif()

# ============================================================
# Executable
# ============================================================

add_executable(${PROJECT_NAME} MACOSX_BUNDLE)

target_sources(${PROJECT_NAME}
    PRIVATE
        MyAppQt.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
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

# ============================================================
# Install main executable
# ============================================================

if (APPLE)
    install(
        TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
    )
else()
    install(
        TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION .  # Changed to root
    )
endif()

# ============================================================
# macOS deployment
# ============================================================

if (APPLE)
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
            COMMENT "Running macdeployqt"
        )
    endif()
endif()

# ============================================================
# Windows deployment
# ============================================================

if (WIN32)
    # Find DLLs portably
    find_file(LIBARCHIVE_DLL libarchive-13.dll REQUIRED)
    find_file(LIBEXPAT_DLL libexpat-1.dll REQUIRED)

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
                    \"\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.exe\"  # Updated for root
        )
    ")

    # Install dynamic DLLs to root
    install(FILES
        ${LIBARCHIVE_DLL}
        ${LIBEXPAT_DLL}
        DESTINATION .
    )
endif()

# ============================================================
# Linux (optional)
# ============================================================

if (UNIX AND NOT APPLE)
    find_program(LINUXDEPLOYQT_EXECUTABLE linuxdeployqt HINTS /usr/bin)

    if (LINUXDEPLOYQT_EXECUTABLE)
        add_custom_command(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMAND ${LINUXDEPLOYQT_EXECUTABLE}
                    $<TARGET_FILE:${PROJECT_NAME}>
                    --deploy-deps-only
            COMMENT "Running linuxdeployqt"
        )
    endif()
endif()
