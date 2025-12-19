

configure_file(version.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)

if(WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/packaging/LogViewer.rc.in ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.rc)
    configure_file(${CMAKE_SOURCE_DIR}/packaging/LogViewer.manifest ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.manifest COPYONLY)
    configure_file(${CMAKE_SOURCE_DIR}/packaging/LogViewer.ico ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.ico COPYONLY)
endif()

add_executable(${PROJECT_NAME} MACOSX_BUNDLE)

target_sources(${PROJECT_NAME}
    PRIVATE
        MyAppQt.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp
)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/LogViewer.rc)
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        application_core
)


set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)


##########Deployment#######
if(APPLE)
    configure_file(${CMAKE_SOURCE_DIR}/packaging/MacOSXBundleInfo.plist.in ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)
    set_target_properties(${PROJECT_NAME} PROPERTIES
         MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    )
    # Deploy Qt libraries into the bundle
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS /opt/homebrew/opt/qt/bin)
    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${MACDEPLOYQT_EXECUTABLE} ${CMAKE_BINARY_DIR}/bin/LogViewer.app
            COMMENT "Running macdeployqt to bundle Qt libraries"
        )
    endif()
endif()

# Deploy Qt libraries for Windows
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS $ENV{QT_DIR}/bin)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE} ${CMAKE_BINARY_DIR}/bin/LogViewer.exe
            COMMENT "Running windeployqt to deploy Qt libraries"
        )
    endif()
endif()

# Deploy Qt libraries for Linux
if(UNIX AND NOT APPLE)
    find_program(LINUXDEPLOYQT_EXECUTABLE linuxdeployqt HINTS /usr/bin)
    if(LINUXDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${LINUXDEPLOYQT_EXECUTABLE} ${CMAKE_BINARY_DIR}/bin/LogViewer --deploy-deps-only
            COMMENT "Running linuxdeployqt to deploy Qt libraries"
        )
    endif()
endif()

