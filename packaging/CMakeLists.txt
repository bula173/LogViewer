#------------------------------------------------------------------------------
# CPACK

set(CPACK_PACKAGE_VENDOR "mkwiatko")
if(USE_QT)
    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}-Qt")
elseif(USE_WXWIDGETS)
    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}-wx")
else()
    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
endif()
set(CPACK_PACKAGE_CONTACT "mkwiatko <marcinkwiatkowski173@gmail.com>")
set(HOMEPAGE "https://github.com/bula173/LogViewer")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Log Viewer is a simple log viewer application.")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/license.md")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/packages")


# set human names to exetuables
set(CPACK_PACKAGE_EXECUTABLES "LogViewer" "LogViewer")
set(CPACK_CREATE_DESKTOP_LINKS "LogViewer")

set(CPACK_COMPONENT_INSTALL ON)
set(CPACK_MONOLITHIC_INSTALL OFF)

# Set components based on what's actually being built
set(CPACK_COMPONENTS_ALL common)

# Add framework-specific components only if they're being built
if(USE_WXWIDGETS)
    list(APPEND CPACK_COMPONENTS_ALL wx_app)
elseif(USE_QT)
    list(APPEND CPACK_COMPONENTS_ALL qt_app)
endif()

set(CPACK_COMPONENT_COMMON_DISPLAY_NAME "Shared Resources")
set(CPACK_COMPONENT_COMMON_DESCRIPTION
    "Shared configuration, icons, and supporting files.")
set(CPACK_COMPONENT_COMMON_HIDDEN TRUE)

# Only define component descriptions if they're being built
if(USE_WXWIDGETS)
    set(CPACK_COMPONENT_WX_APP_DISPLAY_NAME "LogViewer (wxWidgets)")
    set(CPACK_COMPONENT_WX_APP_DESCRIPTION
        "Classic wxWidgets-based desktop application.")
    set(CPACK_COMPONENT_WX_APP_DEPENDS common)
endif()

if(USE_QT)
    set(CPACK_COMPONENT_QT_APP_DISPLAY_NAME "LogViewer (Qt)")
    set(CPACK_COMPONENT_QT_APP_DESCRIPTION
        "Modern Qt-based desktop application.")
    set(CPACK_COMPONENT_QT_APP_DEPENDS common)
endif()

# Disable automatic stripping on Windows (causes issues with MSYS2 paths)
# Files are already optimized with -O3 in Release builds
if(WIN32)
    set(CPACK_STRIP_FILES FALSE)
else()
    set(CPACK_STRIP_FILES TRUE)
endif()

if(WIN32 AND NOT UNIX)
    #--------------------------------------------------------------------------
    # Windows specific - NSIS installer only
    find_program(NSIS_PATH makensis HINTS C:/msys64/mingw64/bin)
    if(NSIS_PATH)
        set(CPACK_GENERATOR "NSIS")
        message(STATUS "Package generation - Windows")
        message(STATUS "   + NSIS                                 YES (${NSIS_PATH})")
        
        set(CPACK_NSIS_DISPLAY_NAME ${CPACK_PACKAGE_NAME})

        if (USE_WXWIDGETS)
            set(CPACK_GUI_EXE_NAME "${PROJECT_NAME}Wx")
            set(CPACK_GUI_DISPLAY_NAME "${PROJECT_NAME} (wxWidgets)")
        elseif (USE_QT)
            set(CPACK_GUI_EXE_NAME "${PROJECT_NAME}Qt")
            set(CPACK_GUI_DISPLAY_NAME "${PROJECT_NAME} (Qt)")
        endif()

        # Icon of the installer
        set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\LogViewer.ico")
        set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\LogViewer.ico")
        # Install/Uninstall icons: point to the right exe (Qt: LogViewerQt.exe, wx: LogViewerWx.exe)
        set(CPACK_NSIS_INSTALLED_ICON_NAME "${CPACK_GUI_EXE_NAME}.exe")

        
        set(CPACK_NSIS_CONTACT "${CPACK_PACKAGE_CONTACT}")
        set(CPACK_NSIS_MODIFY_PATH OFF)
        
        # Create start menu and desktop shortcuts
        set(CPACK_NSIS_CREATE_ICONS_EXTRA
            "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${CPACK_GUI_DISPLAY_NAME}.lnk' '$INSTDIR\\\\${CPACK_GUI_EXE_NAME}.exe'
            CreateShortCut '$DESKTOP\\\\${CPACK_GUI_DISPLAY_NAME}.lnk' '$INSTDIR\\\\${CPACK_GUI_EXE_NAME}.exe'"
        )
        set(CPACK_NSIS_DELETE_ICONS_EXTRA
            "Delete '$SMPROGRAMS\\\\$START_MENU\\\\${CPACK_GUI_DISPLAY_NAME}.lnk'
            Delete '$DESKTOP\\\\${CPACK_GUI_DISPLAY_NAME}.lnk'"
        )
    else()
        message(FATAL_ERROR "NSIS not found! Install with: pacman -S mingw-w64-x86_64-nsis")
    endif()

    # Code Signing Configuration
    # To enable code signing, set these CMake variables:
    # - CODE_SIGN_CERTIFICATE: Path to your .pfx certificate file
    # - CODE_SIGN_PASSWORD: Password for the certificate
    # - CODE_SIGN_TIMESTAMP_URL: Timestamp server (default: DigiCert)
    #
    # For self-signed certificates, see CODE_SIGNING_README.md
    #
    # Example usage:
    # cmake -DCODE_SIGN_CERTIFICATE="C:/path/to/cert.pfx" -DCODE_SIGN_PASSWORD="password" ..
    #
    # To create a self-signed certificate for development:
    # Run: packaging\create_self_signed_cert.bat (Windows)
    # Then configure with the generated .pfx file
    set(CODE_SIGN_CERTIFICATE "" CACHE FILEPATH "Path to code signing certificate (.pfx file)")
    set(CODE_SIGN_PASSWORD "" CACHE STRING "Password for code signing certificate")
    set(CODE_SIGN_TIMESTAMP_URL "http://timestamp.digicert.com" CACHE STRING "Timestamp server URL")
    
    if(CODE_SIGN_CERTIFICATE)
        find_program(SIGNTOOL_EXECUTABLE signtool PATHS "C:/Program Files (x86)/Windows Kits/10/bin" "C:/Program Files/Windows Kits/10/bin")
        if(SIGNTOOL_EXECUTABLE)
            message(STATUS "   + Code Signing                        YES (${SIGNTOOL_EXECUTABLE})")
            
            # Sign the executable after building
            if(USE_QT)
                set(TARGET_TO_SIGN "${PROJECT_NAME}Qt")
            elseif(USE_WXWIDGETS)
                set(TARGET_TO_SIGN "${PROJECT_NAME}Wx")
            endif()
            
            add_custom_command(TARGET ${TARGET_TO_SIGN} POST_BUILD
                COMMAND ${SIGNTOOL_EXECUTABLE} sign /f "${CODE_SIGN_CERTIFICATE}" 
                        /p "${CODE_SIGN_PASSWORD}" 
                        /t "${CODE_SIGN_TIMESTAMP_URL}"
                        /d "LogViewer Application"
                        /du "https://github.com/bula173/LogViewer"
                        "$<TARGET_FILE:${TARGET_TO_SIGN}>"
                COMMENT "Signing executable with code signing certificate..."
                VERBATIM
            )
        else()
            message(WARNING "Code signing certificate specified but signtool.exe not found. Install Windows SDK.")
        endif()
    else()
        message(STATUS "   + Code Signing                        NO (certificate not specified)")
    endif()

    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\LogViewer.png")


    # Configure RC file to replace @CMAKE_SOURCE_DIR@ with actual path
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/LogViewer.rc.in
        ${PROJECT_BINARY_DIR}/packaging/LogViewer.rc
        @ONLY
    )
    
    # Also copy icon and manifest to build directory
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/LogViewer.ico
        ${PROJECT_BINARY_DIR}/packaging/LogViewer.ico
        COPYONLY
    )
    
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/LogViewer.manifest
        ${PROJECT_BINARY_DIR}/packaging/LogViewer.manifest
        @ONLY
    )
elseif(APPLE)
    #--------------------------------------------------------------------------
    # Apple specific
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")

    set(MACOSX_BUNDLE_BUNDLE_NAME ${CPACK_PACKAGE_NAME})
    set(MACOSX_BUNDLE_BUNDLE_GUI_IDENTIFIER "com.LogViewer.LogViewer")
    set(MACOSX_BUNDLE_ICON_FILE ${PROJECT_SOURCE_DIR}/packaging/LogViewer.icns)
    set(MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/packaging/MacOSXBundleInfo.plist.in)
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
    
    set_source_files_properties(${PROJECT_SOURCE_DIR}/packaging/LogViewer.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    
    set(CPACK_DMG_VOLUME_NAME "LogViewer")
    
    # Only use DS_Store setup script if not in CI environment
    # The AppleScript fails in headless GitHub Actions runners
    if(NOT DEFINED ENV{CI} AND NOT DEFINED ENV{GITHUB_ACTIONS})
        set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${PROJECT_SOURCE_DIR}/packaging/DS_Store.scpt")
        message(STATUS "DMG will use custom DS_Store layout (interactive mode)")
    else()
        message(STATUS "DMG will use default layout (CI mode - skipping AppleScript)")
        # Use basic DMG settings for CI/headless environments
        set(CPACK_DMG_FORMAT "UDZO")  # Compressed format
        set(CPACK_DMG_BACKGROUND_IMAGE "")  # No background image in CI
    endif()
    
    # Common DMG settings
    set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE ON)
    set(CPACK_OSX_PACKAGE_VERSION "10.6") #min package version
    
else()
    #-----------------------------------------------------------------------------
    # Linux specific
    set(CPACK_GENERATOR "DEB;TBZ2;TXZ")
    message(STATUS "Package generation - UNIX")
    message(STATUS "   + DEB                                  YES ")
    message(STATUS "   + TBZ2                                 YES ")
    message(STATUS "   + TXZ                                  YES ")

    find_program(RPMBUILD_PATH rpmbuild)
    if(RPMBUILD_PATH)
        message(STATUS "   + RPM                                  YES ")
        set(CPACK_GENERATOR "${CPACK_GENERATOR};RPM")
        set(CPACK_RPM_PACKAGE_LICENSE "MIT")
        # set(CPACK_RPM_PACKAGE_REQUIRES "gtkmm30")
        # exclude folders which clash with default ones
        set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST
            ${CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST}
            /usr
            /usr/bin
            /usr/share
            /usr/share/applications
            /usr/share/doc
            /usr/share/icons
            /usr/share/icons/hicolor
            /usr/share/icons/hicolor/256x256
            /usr/share/icons/hicolor/256x256/apps
            /usr/share/icons/gnome
            /usr/share/icons/gnome/256x256
            /usr/share/icons/gnome/256x256/apps)
    else()
        message(STATUS "   + RPM                                  NO ")
    endif()

    # TODO do this better
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${HOMEPAGE}")
    # set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")
    # set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgtkmm-3.0")



    # Icon and app shortcut for Linux systems
    # Note: .desktop file must have same name as executable
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LogViewer.desktop
        DESTINATION share/applications/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        COMPONENT common)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LogViewer.png
        DESTINATION share/icons/hicolor/256x256/apps/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        COMPONENT common)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LogViewer.png
        DESTINATION share/icons/gnome/256x256/apps/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        COMPONENT common)
    # License file
    install(FILES ${PROJECT_SOURCE_DIR}/license.md
        DESTINATION share/doc/${PROJECT_NAME}/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        RENAME copyright
        COMPONENT common)
    # set package icon
    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/LogViewer.png")
endif()

#------------------------------------------------------------------------------

message(STATUS "")
message(STATUS "    == Packaging overview for ${PROJECT_NAME} ==")
message(STATUS "Version:              ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} ${VERSION_TYPE} @ ${VERSION_HOST}")
message(STATUS "Install prefix:       ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Compiler:             ${CMAKE_CXX_COMPILER}")
message(STATUS "CMake generator:      ${CMAKE_GENERATOR}")
message(STATUS "CPACK_PACKAGE_DIRECTORY: ${CPACK_PACKAGE_DIRECTORY}")
message(STATUS "")
# and include CPack, so we get target for packages
include(CPack)

if(CMAKE_CPACK_COMMAND)
    set(_cpack_config "${CMAKE_BINARY_DIR}/CPackConfig.cmake")
    if(APPLE)
        set(_split_generator "DragNDrop")
    elseif(WIN32)
        set(_split_generator "NSIS")
    else()
        set(_split_generator "TGZ")
    endif()

    function(_add_split_package target component_list flavor)
        add_custom_target(${target}
            COMMAND "${CMAKE_CPACK_COMMAND}" --config "${_cpack_config}"
                -C "$<CONFIG>"
                -G "${_split_generator}"
                "-DCPACK_COMPONENTS_ALL=${component_list}"
                "-DCPACK_PACKAGE_NAME=LogViewer-${flavor}"
                "-DCPACK_PACKAGE_FILE_NAME=LogViewer-${flavor}-${PROJECT_VERSION}"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            COMMENT "Creating ${flavor} installer via CPack"
            VERBATIM)
    endfunction()

    # Only create packaging targets for frameworks that are being built
    if(USE_WXWIDGETS)
        _add_split_package(package_wx "wx_app\;common" "wx")
    endif()
    
    if(USE_QT)
        _add_split_package(package_qt "qt_app\;common" "qt")
    endif()
    
endif()

#-------------------------------------------------------------------------------
# Installation rules (for CPack)
