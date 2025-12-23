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

add_executable(${PROJECT_NAME})

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if (APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
endif()

target_sources(${PROJECT_NAME}
    PRIVATE
        MyAppQt.cpp
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
        COMPONENT application
    )
else()
    install(
        TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION . 
        COMPONENT application
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
    "
    COMPONENT application
    )

    # Deploy MinGW runtime dependencies at install time    
    install(CODE [[
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES $<TARGET_FILE:LogViewer>
            RESOLVED_DEPENDENCIES_VAR _resolved_deps
            UNRESOLVED_DEPENDENCIES_VAR _unresolved_deps
            DIRECTORIES "C:/msys64/mingw64/bin"  # Adjust if using UCRT64
        )
        foreach(_file ${_resolved_deps})
            file(INSTALL
                DESTINATION "${CMAKE_INSTALL_PREFIX}"
                TYPE SHARED_LIBRARY
                FILES "${_file}"
            )
        endforeach()
    ]]
    COMPONENT application
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
