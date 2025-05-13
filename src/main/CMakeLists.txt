
message(STATUS "Building LogViewer::main")
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# Adds convenience methods, see cmake/cleanCppExtensions.cmake

if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/cleanCppExtensions.cmake")
    include(cleanCppExtensions)
else()
    message(WARNING "cleanCppExtensions.cmake not found, run and debug targets not available")  
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/sanitizers.cmake")
    include(sanitizers)
else()
    message(WARNING "sanitizers.cmake not found, sanitizers not available")
endif()

# configure version.cpp.in with selected version
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/version.cpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/version.cpp"
  )
# If you want to have date and time of the build, your targets has to depend on this.
# This will force recompilation of version.o and thus forcing gcc to update __DATE__ macro.
add_custom_target(versionFileTouchForRebuild
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")
#-------------------------------------------------------------------------------
# exampleApp executable setup

set(ICON_RESOURCE "")
if(WIN32) #windows icon resource file, provided from /packaging directory
    set(ICON_RESOURCE "${PROJECT_BINARY_DIR}/exampleApp.icon.rc")
elseif (APPLE) #osx icon ressource file, provided from /packaging directory
    set(ICON_RESOURCE "${PROJECT_SOURCE_DIR}/packaging/exampleApp.icns")
endif()


if(APPLE)

    # create bundle on apple compiles
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${CMAKE_CURRENT_SOURCE_DIR}/MyApp.cpp)

    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_BUNDLE_GUI_IDENTIFIER "com.LogViewer.LogViewer")
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE exampleApp.icns)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/packaging/MacOSXBundleInfo.plist.in)
    set_source_files_properties(${PROJECT_SOURCE_DIR}/packaging/exampleApp.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
else()
    # the WIN32 is needed for Windows in order for it to look for WinMain
    # instead of the main function. This is ignored on other systems,
    # so it works on all platforms
    add_executable(${PROJECT_NAME} WIN32 ${CMAKE_CURRENT_SOURCE_DIR}/MyApp.cpp ${PROJECT_SOURCE_DIR}/packaging/main.exe.manifest)
endif()

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION bin)

# Using macro from cmake/cleanCpppExtensions.cpp included on the top
# Provides ${PROJECT_NAME}-run and ${PROJECT_NAME}-dbg
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/cleanCppExtensions.cmake")
    message(STATUS "Using cleanCppExtensions.cmake")
    include(cleanCppExtensions)
else()
    message(WARNING "cleanCppExtensions.cmake not found, run and debug targets not available")
endif()
# Add run and debug targets


# Link wxWidgets
target_link_libraries(${PROJECT_NAME} PRIVATE application)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(${PROJECT_NAME} application)


#-------------------------------------------------------------------------------
# Copy MINGW needed libraries for building on windows

if(MINGW)
    message(STATUS "MinGW detected")
    get_filename_component(GCC_PATH ${CMAKE_C_COMPILER} PATH)
    if(${GCC_PATH} MATCHES "mingw64/bin")
        set(libgcc "libgcc_s_seh-1.dll") #64bit
        message(STATUS "  64bit dlls is building")
    else()
        set(libgcc "libgcc_s_dw2-1.dll") #32bit
        message(STATUS "  32bit dlls is building")
    endif()

    install(FILES ${GCC_PATH}/${libgcc}
        ${GCC_PATH}/libwinpthread-1.dll
        ${GCC_PATH}/libstdc++-6.dll
        DESTINATION ./bin/
    )
endif(MINGW)


#------------------------------------------------------------------------------
# Other MISC targets

# Adds misc targets: format, cppcheck, tidy, see cmake/cleanCppExtensions.cmake
addMiscTargets()
