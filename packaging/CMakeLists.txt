#------------------------------------------------------------------------------
# CPACK

set(CPACK_PACKAGE_VENDOR "mkwiatko")
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
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
set(CPACK_STRIP_FILES TRUE)

if(WIN32 AND NOT UNIX)
    #--------------------------------------------------------------------------
    # Windows specific
    set(CPACK_GENERATOR "STGZ;ZIP")
    message(STATUS "Package generation - Windows")
    message(STATUS "   + STGZ                                 YES ")
    message(STATUS "   + ZIP                                  YES ")

    # NSIS windows installer
    find_program(NSIS_PATH nsis PATH_SUFFIXES nsis)
    if(NSIS_PATH)
        set(CPACK_GENERATOR "${CPACK_GENERATOR};NSIS")
        message(STATUS "   + NSIS                                 YES ")
        # Note: There is a bug in NSI that does not handle full unix paths properly. Make
        # sure there is at least one set of four (4) backlasshes.
        set(CPACK_NSIS_DISPLAY_NAME ${CPACK_PACKAGE_NAME})
        # Icon of the installer
        set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\exampleApp.ico")
        # set(CPACK_NSIS_HELP_LINK "http:\\\\\\\\www.my-project-home-page.org")
        # set(CPACK_NSIS_URL_INFO_ABOUT "http:\\\\\\\\www.my-personal-home-page.com")
        set(CPACK_NSIS_CONTACT "${CPACK_PACKAGE_CONTACT}")
        set(CPACK_NSIS_MODIFY_PATH ON)
    else()
        message(STATUS "   + NSIS                                 NO ")
    endif()

    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\exampleApp.png")

    # Configure file with right path, place the result to PROJECT_BINARY_DIR.
    # When ${PROJECT_BINARY_DIR}/exampleApp.icon.rc is added to an executable
    # it will have icon specified in exampleApp.icon.in.rc
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/exampleApp.icon.in.rc
        ${PROJECT_BINARY_DIR}/exampleApp.icon.rc)

elseif(APPLE)
    #--------------------------------------------------------------------------
    # Apple specific
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/")

    set(MACOSX_BUNDLE_BUNDLE_NAME ${CPACK_PACKAGE_NAME})
    set(MACOSX_BUNDLE_BUNDLE_GUI_IDENTIFIER "com.LogViewer.LogViewer")
    set(MACOSX_BUNDLE_ICON_FILE ${PROJECT_SOURCE_DIR}/packaging/exampleApp.icns)
    set(MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/packaging/MacOSXBundleInfo.plist.in)
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
    
    set_source_files_properties(${PROJECT_SOURCE_DIR}/packaging/exampleApp.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    
    set(CPACK_DMG_VOLUME_NAME "LogViewer")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${PROJECT_SOURCE_DIR}/packaging/DS_Store.scpt") 
    set(CPACK_DMG_BACKGROUND_IMAGE "${PROJECT_SOURCE_DIR}/packaging/dmg_background.png")
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
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/exampleApp.desktop
        DESTINATION share/applications/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/exampleApp.png
        DESTINATION share/icons/hicolor/256x256/apps/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/exampleApp.png
        DESTINATION share/icons/gnome/256x256/apps/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    # License file
    install(FILES ${PROJECT_SOURCE_DIR}/license.md
        DESTINATION share/doc/${PROJECT_NAME}/
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        RENAME copyright)
    # set package icon
    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/exampleApp.png")
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



