message(STATUS "Building LogViewer::ssh_parser_plugin (optional)")

set(PLUGIN_NAME ssh_parser_plugin)
set(PLUGIN_ID "ssh_parser")
set(PLUGIN_VERSION "1.0.0")
set(PLUGIN_DISPLAY_NAME "SSH Parser Plugin")
set(PLUGIN_API_VERSION "1.0.0")

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
    SSHTerminalWidget.cpp
)

set(SSH_PARSER_HEADERS
    SSHConnection.hpp
    SSHTextParser.hpp
    SSHLogSource.hpp
    SSHParserPlugin.hpp
    SSHConnectionWidget.hpp
    SSHTerminalWidget.hpp
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
    PREFIX ""  # Don't add 'lib' prefix
    OUTPUT_NAME "${PLUGIN_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# Add preprocessor definition to enable SSH features
target_compile_definitions(${PLUGIN_NAME} PUBLIC LOGVIEWER_ENABLE_SSH_PARSER)

# Install the plugin to the plugins directory
install(TARGETS ${PLUGIN_NAME}
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins
)

# Packaging and config generation
set(PLUGIN_DEPENDENCIES_JSON "[]")

if(WIN32)
    set(PLUGIN_EXTENSION ".dll")
elseif(APPLE)
    set(PLUGIN_EXTENSION ".dylib")
else()
    set(PLUGIN_EXTENSION ".so")
endif()

set(PLUGIN_ENTRY "${PLUGIN_NAME}${PLUGIN_EXTENSION}")

if(LIBSSH_LIBRARIES)
    get_filename_component(LIBSSH_DIR ${LIBSSH_LIBRARIES} DIRECTORY)
    
    # On Windows, LIBSSH_LIBRARIES is the import lib (.dll.a), but we need the actual DLL
    if(WIN32)
        # Will be set later when we find the DLL
        set(LIBSSH_BASENAME "libssh.dll")
    else()
        get_filename_component(LIBSSH_BASENAME ${LIBSSH_LIBRARIES} NAME)
    endif()
    
    set(PLUGIN_DEPENDENCIES_JSON "[\"${LIBSSH_BASENAME}\"]")
endif()

set(PLUGIN_CONFIG_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/config.json.in")
set(PLUGIN_CONFIG "${CMAKE_CURRENT_BINARY_DIR}/config.json")
# Set plugin output to build/plugins directory
set_target_properties(${PLUGIN_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

configure_file(${PLUGIN_CONFIG_TEMPLATE} ${PLUGIN_CONFIG} @ONLY)

# Create plugin package in build directory
set(PLUGIN_PACKAGE_DIR "${CMAKE_BINARY_DIR}/plugins/${PLUGIN_NAME}_package")
file(MAKE_DIRECTORY ${PLUGIN_PACKAGE_DIR})

add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PLUGIN_NAME}>
        ${PLUGIN_PACKAGE_DIR}/${PLUGIN_ENTRY}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${PLUGIN_CONFIG}
        ${PLUGIN_PACKAGE_DIR}/config.json
    COMMENT "Preparing ${PLUGIN_NAME} artifacts"
)

if(LIBSSH_LIBRARIES)
    if(WIN32)
        # Clear the cache variable to force a fresh search
        unset(LIBSSH_DLL CACHE)
        
        # Search for libssh.dll in multiple locations
        find_file(LIBSSH_DLL
            NAMES libssh.dll ssh.dll
            PATHS 
                ${LIBSSH_DIR}
                ${LIBSSH_DIR}/../bin
                ${LIBSSH_DIR}/../../bin
                /mingw64/bin
                $ENV{MINGW_PREFIX}/bin
            NO_DEFAULT_PATH
        )
        
        if(LIBSSH_DLL)
            add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${LIBSSH_DLL}
                    ${PLUGIN_PACKAGE_DIR}/
                COMMENT "Copying libssh.dll to package directory"
            )
            message(STATUS "Found libssh DLL at: ${LIBSSH_DLL}")
        else()
            message(WARNING "libssh.dll not found for packaging. Searched in: ${LIBSSH_DIR}, ${LIBSSH_DIR}/../bin")
        endif()
    else()
        add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${LIBSSH_LIBRARIES}
                ${PLUGIN_PACKAGE_DIR}/
            COMMENT "Copying libssh to package directory"
        )
    endif()
endif()

set(PACKAGE_FILES "${PLUGIN_ENTRY}" "config.json")
if(LIBSSH_BASENAME)
    list(APPEND PACKAGE_FILES "${LIBSSH_BASENAME}")
endif()

add_custom_command(TARGET ${PLUGIN_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar "cf"
        "${CMAKE_BINARY_DIR}/plugins/${PLUGIN_NAME}.zip" --format=zip
        ${PACKAGE_FILES}
    WORKING_DIRECTORY ${PLUGIN_PACKAGE_DIR}
    COMMENT "Packaging ${PLUGIN_NAME}.zip"
    VERBATIM
)

message(STATUS "SSH Parser Plugin: Build configured")
