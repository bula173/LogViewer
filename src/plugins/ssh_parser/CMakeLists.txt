message(STATUS "Building LogViewer::ssh_parser_plugin (optional)")

set(PLUGIN_NAME ssh_parser_plugin)


# SSH parser plugin - optional feature for specific customers
# This plugin provides SSH connectivity and text log parsing capabilities

# Find Qt6 for UI components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

set(SSH_PARSER_SRCS
    SSHConnection.cpp
    SSHTextParser.cpp
    SSHLogSource.cpp
    SSHParserPlugin.cpp
    SSHConnectionWidget.cpp
)

set(SSH_PARSER_HEADERS
    SSHConnection.hpp
    SSHTextParser.hpp
    SSHLogSource.hpp
    SSHParserPlugin.hpp
    SSHConnectionWidget.hpp
)

# Find libssh - required dependency (check BEFORE creating target)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBSSH libssh)
    
    # pkg-config returns -lssh which needs full path
    if(LIBSSH_FOUND AND LIBSSH_LIBRARIES)
        find_library(LIBSSH_LIBRARY_PATH
            NAMES ${LIBSSH_LIBRARIES}
            PATHS ${LIBSSH_LIBRARY_DIRS}
            NO_DEFAULT_PATH
            STATIC
        )
        if(LIBSSH_LIBRARY_PATH)
            set(LIBSSH_LIBRARIES ${LIBSSH_LIBRARY_PATH})
        endif()
    endif()
endif()

if(NOT LIBSSH_FOUND)
    # Try to find libssh manually
    find_path(LIBSSH_INCLUDE_DIR libssh/libssh.h
        PATHS
            /usr/include
            /usr/local/include
            /opt/homebrew/include
            ${CMAKE_PREFIX_PATH}/include
    )
    
    find_library(LIBSSH_LIBRARY
        NAMES ssh libssh
        PATHS
            /opt/homebrew/lib
            /usr/local/lib
            /usr/lib
            ${CMAKE_PREFIX_PATH}/lib
        NO_DEFAULT_PATH
        STATIC
    )
    
    if(NOT LIBSSH_LIBRARY)
        find_library(LIBSSH_LIBRARY
            NAMES ssh libssh
        )
    endif()
    
    if(LIBSSH_INCLUDE_DIR AND LIBSSH_LIBRARY)
        set(LIBSSH_FOUND TRUE)
        set(LIBSSH_INCLUDE_DIRS ${LIBSSH_INCLUDE_DIR})
        set(LIBSSH_LIBRARIES ${LIBSSH_LIBRARY})
        message(STATUS "Found libssh: ${LIBSSH_LIBRARY}")
    else()
        message(WARNING "libssh not found. SSH parser will not be built.")
        message(WARNING "  Install with: brew install libssh (macOS) or apt-get install libssh-dev (Linux)")
        return()
    endif()
endif()

# Create the SSH parser as a shared library plugin (only if libssh found)
add_library(${PLUGIN_NAME} SHARED ${SSH_PARSER_SRCS} ${SSH_PARSER_HEADERS})

target_include_directories(${PLUGIN_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${LIBSSH_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PLUGIN_NAME}
    PUBLIC
        application_core
        ${LIBSSH_LIBRARIES}
        Qt6::Core
        Qt6::Widgets
    PRIVATE
        spdlog::spdlog_header_only
)

# Enable Qt MOC for widget processing
set_target_properties(${PLUGIN_NAME} PROPERTIES
    AUTOMOC ON
)

# Add preprocessor definition to enable SSH features
target_compile_definitions(${PLUGIN_NAME} PUBLIC LOGVIEWER_ENABLE_SSH_PARSER)

# Set plugin output properties
set_target_properties(${PLUGIN_NAME} PROPERTIES
    PREFIX ""  # Don't add 'lib' prefix
    OUTPUT_NAME "${PLUGIN_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# Install the plugin to the plugins directory
install(TARGETS ${PLUGIN_NAME}
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins
)

# Copy to dist/plugins for development
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    set(PLUGIN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/dist/plugins")
    file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})

    if(WIN32)
        set(PLUGIN_EXTENSION ".dll")
    elseif(APPLE)
        set(PLUGIN_EXTENSION ".dylib")
    else()
        set(PLUGIN_EXTENSION ".so")
    endif()

    # Copy the plugin
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:${PLUGIN_NAME}>
            ${PLUGIN_OUTPUT_DIR}/${PLUGIN_NAME}${PLUGIN_EXTENSION}
        COMMENT "Copying ${PLUGIN_NAME} to ${PLUGIN_OUTPUT_DIR}"
    )

    # Copy libssh dependency
    if(LIBSSH_LIBRARIES)
        # Find the actual library file (handle both .dll.a and .dll on Windows)
        get_filename_component(LIBSSH_DIR ${LIBSSH_LIBRARIES} DIRECTORY)
        get_filename_component(LIBSSH_NAME ${LIBSSH_LIBRARIES} NAME_WE)
        
        if(WIN32)
            # On Windows, find the actual .dll (not the import library .dll.a)
            find_file(LIBSSH_DLL
                NAMES libssh.dll ssh.dll
                PATHS ${LIBSSH_DIR} ${LIBSSH_DIR}/../bin
                NO_DEFAULT_PATH
            )
            if(LIBSSH_DLL)
                add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${LIBSSH_DLL}
                        ${PLUGIN_OUTPUT_DIR}/
                    COMMENT "Copying libssh.dll to ${PLUGIN_OUTPUT_DIR}"
                )
            else()
                message(WARNING "libssh.dll not found for packaging")
            endif()
        elseif(APPLE)
            # On macOS, copy the dylib
            add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${LIBSSH_LIBRARIES}
                    ${PLUGIN_OUTPUT_DIR}/
                COMMENT "Copying libssh to ${PLUGIN_OUTPUT_DIR}"
            )
        else()
            # On Linux, copy the .so
            add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${LIBSSH_LIBRARIES}
                    ${PLUGIN_OUTPUT_DIR}/
                COMMENT "Copying libssh to ${PLUGIN_OUTPUT_DIR}"
            )
        endif()
    endif()

    # Copy plugin manifest
    add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/ssh_parser_plugin.json
            ${PLUGIN_OUTPUT_DIR}/ssh_parser_plugin.json
        COMMENT "Copying plugin manifest to ${PLUGIN_OUTPUT_DIR}"
    )
endif()

# Add custom target to create plugin package
add_custom_target(package_ssh_parser
    COMMAND ${CMAKE_COMMAND} -E tar "cf"
        "${PLUGIN_OUTPUT_DIR}/ssh_parser_plugin.zip" --format=zip
        "${PLUGIN_NAME}${PLUGIN_EXTENSION}"
        "libssh.dll"
        "ssh_parser_plugin.json"
    WORKING_DIRECTORY ${PLUGIN_OUTPUT_DIR}
    DEPENDS ${PLUGIN_NAME}
    COMMENT "Creating ssh_parser_plugin package in ${PLUGIN_OUTPUT_DIR}"
)

message(STATUS "SSH Parser Plugin: ENABLED")
message(STATUS "  libssh include: ${LIBSSH_INCLUDE_DIRS}")
message(STATUS "  libssh library: ${LIBSSH_LIBRARIES}")
message(STATUS "  Plugin output: ${CMAKE_BINARY_DIR}/plugins")
