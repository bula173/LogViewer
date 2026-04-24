 # copy_dlls.cmake - Use CMake's native dependency resolver (no ldd needed)

# Suppress CMP0207: file(GET_RUNTIME_DEPENDENCIES) path normalisation (CMake 3.28+)
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

set(EXE_PATH "${ARGV0}")
set(DEST_DIR "${ARGV1}")
set(COMPILER_PATH "${ARGV2}")
set(EXCLUDE_APP "${ARGV3}")

# Validate inputs
if(NOT EXISTS "${EXE_PATH}")
    message(WARNING "Executable not found: ${EXE_PATH}")
    return()
endif()

# Detect toolchain bin directory
get_filename_component(COMPILER_DIR "${COMPILER_PATH}" DIRECTORY)
message(STATUS "Analyzing dependencies for: ${EXE_PATH}")
message(STATUS "Toolchain bin: ${COMPILER_DIR}")

# Create destination directory
file(MAKE_DIRECTORY "${DEST_DIR}")

# Use CMake's file(GET_RUNTIME_DEPENDENCIES) to find all DLLs
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${EXE_PATH}"
    RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPS
    UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPS
    DIRECTORIES "${COMPILER_DIR}"
    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
    POST_EXCLUDE_REGEXES ".*[Ss]ystem32/.*\\.dll" ".*[Ww]indows/.*\\.dll"
)

# If an application executable was provided as EXCLUDE_APP (ARGV3), resolve
# its runtime dependencies now and collect their filenames so we can avoid
# copying DLLs that the main application already ships.
set(APP_PROVIDED_DLL_NAMES)
if(EXCLUDE_APP AND EXISTS "${EXCLUDE_APP}")
    # EXCLUDE_APP is expected to be the application executable path
    message(STATUS "EXCLUDE_APP provided: ${EXCLUDE_APP}")
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${EXCLUDE_APP}"
        RESOLVED_DEPENDENCIES_VAR APP_RESOLVED_DEPS
        UNRESOLVED_DEPENDENCIES_VAR APP_UNRESOLVED_DEPS
        DIRECTORIES "${COMPILER_DIR}"
        PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
        POST_EXCLUDE_REGEXES ".*[Ss]ystem32/.*\\.dll" ".*[Ww]indows/.*\\.dll"
    )
    foreach(_a IN LISTS APP_RESOLVED_DEPS)
        get_filename_component(_aname "${_a}" NAME)
        list(APPEND APP_PROVIDED_DLL_NAMES "${_aname}")
    endforeach()
    # Always report what was discovered for easier debugging
    if(APP_RESOLVED_DEPS)
        message(STATUS "Application resolved dependencies (${EXCLUDE_APP}): ${APP_RESOLVED_DEPS}")
    else()
        message(STATUS "No resolved dependencies found for application: ${EXCLUDE_APP}")
    endif()
    message(STATUS "Application provided DLLs: ${APP_PROVIDED_DLL_NAMES}")
endif()

# Copy all resolved MinGW/MSYS2 DLLs
set(COPIED_COUNT 0)
foreach(DEP ${RESOLVED_DEPS})
    # Only copy DLLs from the toolchain directory used to build (skip Windows system DLLs)
    # Consider a dependency part of the toolchain if its resolved path contains the compiler directory
    string(FIND "${DEP}" "${COMPILER_DIR}" TOOLCHAIN_MATCH)
    if(NOT TOOLCHAIN_MATCH EQUAL -1)
        get_filename_component(DLL_NAME "${DEP}" NAME)
        # If application provided DLLs are known, skip any dependency whose
        # filename is present in that set.
        if(APP_PROVIDED_DLL_NAMES)
            list(FIND APP_PROVIDED_DLL_NAMES "${DLL_NAME}" _found)
            if(NOT _found EQUAL -1)
                message(STATUS "Skipping (provided by application): ${DLL_NAME}")
                continue()
            endif()
        endif()
        
        if(NOT EXISTS "${DEST_DIR}/${DLL_NAME}")
            file(COPY "${DEP}" DESTINATION "${DEST_DIR}")
            message(STATUS "Copied: ${DLL_NAME}")
            math(EXPR COPIED_COUNT "${COPIED_COUNT} + 1")
        endif()
    endif()
endforeach()

# Warn about unresolved dependencies (optional)
if(UNRESOLVED_DEPS)
    message(STATUS "Unresolved dependencies (may be system DLLs): ${UNRESOLVED_DEPS}")
endif()

message(STATUS "Copied ${COPIED_COUNT} MinGW/MSYS2 DLLs to ${DEST_DIR}")