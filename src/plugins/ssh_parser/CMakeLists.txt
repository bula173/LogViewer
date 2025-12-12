message(STATUS "Building LogViewer::ssh_parser_plugin (optional)")

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

# Create the SSH parser as a shared library plugin
add_library(ssh_parser_plugin SHARED ${SSH_PARSER_SRCS} ${SSH_PARSER_HEADERS})

# Find libssh - required dependency
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBSSH libssh)
    
    # pkg-config returns -lssh which needs full path
    if(LIBSSH_FOUND AND LIBSSH_LIBRARIES)
        find_library(LIBSSH_LIBRARY_PATH
            NAMES ${LIBSSH_LIBRARIES}
            PATHS ${LIBSSH_LIBRARY_DIRS}
            NO_DEFAULT_PATH
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

target_include_directories(ssh_parser_plugin
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/application
        ${LIBSSH_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ssh_parser_plugin
    PUBLIC
        application_core
        ${LIBSSH_LIBRARIES}
        Qt6::Core
        Qt6::Widgets
    PRIVATE
        spdlog::spdlog_header_only
)

# Enable Qt MOC for widget processing
set_target_properties(ssh_parser_plugin PROPERTIES
    AUTOMOC ON
)

# Add preprocessor definition to enable SSH features
target_compile_definitions(ssh_parser_plugin PUBLIC LOGVIEWER_ENABLE_SSH_PARSER)

# Set plugin output properties
set_target_properties(ssh_parser_plugin PROPERTIES
    PREFIX ""  # Don't add 'lib' prefix
    OUTPUT_NAME "ssh_parser_plugin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# Install the plugin to the plugins directory
install(TARGETS ssh_parser_plugin
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins
)

# Copy to dist/plugins for development
if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    set(PLUGIN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/dist/plugins")
    file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})

    add_custom_command(TARGET ssh_parser_plugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:ssh_parser_plugin>
            ${PLUGIN_OUTPUT_DIR}/$<TARGET_FILE_NAME:ssh_parser_plugin>
        COMMENT "Copying ssh_parser_plugin to ${PLUGIN_OUTPUT_DIR}"
    )
endif()

message(STATUS "SSH Parser Plugin: ENABLED")
message(STATUS "  libssh include: ${LIBSSH_INCLUDE_DIRS}")
message(STATUS "  libssh library: ${LIBSSH_LIBRARIES}")
message(STATUS "  Plugin output: ${CMAKE_BINARY_DIR}/plugins")
