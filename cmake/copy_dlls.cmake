 # copy_dlls.cmake - Use CMake's native dependency resolver (no ldd needed)

set(EXE_PATH "${ARGV0}")
set(DEST_DIR "${ARGV1}")
set(COMPILER_PATH "${ARGV2}")

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

# Copy all resolved MinGW/MSYS2 DLLs
set(COPIED_COUNT 0)
foreach(DEP ${RESOLVED_DEPS})
    # Only copy DLLs from the toolchain directory (skip Windows system DLLs)
    string(FIND "${DEP}" "mingw64" TOOLCHAIN_MATCH)
    if(NOT TOOLCHAIN_MATCH EQUAL -1)
        get_filename_component(DLL_NAME "${DEP}" NAME)
        
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