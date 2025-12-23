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

# Correct the path to include the modular CMake files
include(../cmake/EnableSanitizers.cmake)
include(../cmake/Cppcheck.cmake)
include(../cmake/ClangTidy.cmake)

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

if (WIN32)
    # 1. Capture paths before entering the install code string
    get_filename_component(COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    # 2. Deployment Logic
    install(CODE "
        set(COMPILER_DIR \"${COMPILER_BIN_DIR}\")
        
        # --- PART A: Deploy Qt Dependencies ---
        # (Assuming WINDEPLOYQT_EXECUTABLE was found earlier)
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-compiler-runtime
                    \"$<TARGET_FILE:${PROJECT_NAME}>\"
        )

        # --- PART B: Deploy MinGW/Clang Runtime Dependencies ---
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES \"$<TARGET_FILE:${PROJECT_NAME}>\"
            RESOLVED_DEPENDENCIES_VAR _resolved_deps
            UNRESOLVED_DEPENDENCIES_VAR _unresolved_deps
            DIRECTORIES \"\${COMPILER_DIR}\"
            PRE_EXCLUDE_REGEXES 
                \"api-ms-.*\" \"ext-ms-.*\" \"kernel32.*\" \"user32.dll\" 
                \"gdi32.dll\" \"win32u.dll\" \"combase.dll\" \"rpcrt4.dll\" 
                \"msvcp.*\\\\.dll\" \"vcruntime.*\\\\.dll\" \"ucrtbase.dll\" 
                \"d3d.*\\\\.dll\" \"shell32.dll\" \"advapi32.dll\" \"bcrypt.dll\" 
                \"cfgmgr32.dll\" \"crypt32.dll\" \"gdi32full.dll\" \"imm32.dll\" 
                \"msvcrt.dll\" \"ntdll.dll\" \"sechost.dll\" \"shcore.dll\" 
                \"ws2_32.dll\" \"ole32.dll\" \"oleaut32.dll\" \"version.dll\"
            POST_EXCLUDE_REGEXES \"Qt6.*\\\\.dll\"
        )

        foreach(_file IN LISTS _resolved_deps)
            file(INSTALL
                DESTINATION \"\${CMAKE_INSTALL_PREFIX}\"
                TYPE SHARED_LIBRARY
                FILES \"\${_file}\"
            )
        endforeach()

        # Print warning if any non-excluded DLL was not found
        foreach(_file IN LISTS _unresolved_deps)
            message(WARNING \"Dependency not found: \${_file}\")
        endforeach()
    " COMPONENT application)
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
