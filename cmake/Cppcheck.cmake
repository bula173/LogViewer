# cmake/Cppcheck.cmake
#
# Creates a `cppcheck` custom target for static analysis.
#
# Controlled by:
#   ENABLE_CPPCHECK  BOOL  — enable the cppcheck target
#
# Usage:
#   include(cmake/Cppcheck.cmake)       # in root CMakeLists.txt
#   cmake --build --preset linux-cppcheck

option(ENABLE_CPPCHECK "Enable cppcheck static analysis" OFF)

if(NOT ENABLE_CPPCHECK)
    # Create a stub so presets referencing the target don't fail
    if(NOT TARGET cppcheck)
        add_custom_target(cppcheck
            COMMAND ${CMAKE_COMMAND} -E echo "cppcheck is disabled (ENABLE_CPPCHECK=OFF)"
            COMMENT "cppcheck: disabled"
        )
    endif()
    return()
endif()

find_program(CPPCHECK_EXECUTABLE cppcheck)

if(NOT CPPCHECK_EXECUTABLE)
    message(WARNING "[cppcheck] Executable not found — target will be a no-op. "
                    "Install cppcheck or set CPPCHECK_EXECUTABLE.")
    if(NOT TARGET cppcheck)
        add_custom_target(cppcheck
            COMMAND ${CMAKE_COMMAND} -E echo "cppcheck not found"
            COMMENT "cppcheck: not found"
        )
    endif()
    return()
endif()

message(STATUS "[cppcheck] Found: ${CPPCHECK_EXECUTABLE}")

# Optional suppressions file — project can provide one at cmake/cppcheck-suppressions.txt
set(CPPCHECK_SUPPRESSIONS_FILE "${CMAKE_SOURCE_DIR}/cmake/cppcheck-suppressions.txt")

set(CPPCHECK_CMD
    ${CPPCHECK_EXECUTABLE}
    # Analysis scope
    --project=${CMAKE_BINARY_DIR}/compile_commands.json
    # Checks to enable
    --enable=warning,performance,portability,information,style
    --inconclusive
    # Output format
    --template=gcc
    --error-exitcode=1          # Non-zero exit on findings
    # Suppress common false positives
    --suppress=missingInclude   # Missing system/Qt includes
    --suppress=missingIncludeSystem
    --suppress=normalCheckLevelMaxBranches
    --inline-suppr              # Honour // cppcheck-suppress comments in code
    # Exclude generated and third-party code
    -i ${CMAKE_BINARY_DIR}      # Generated files (moc, qrc, etc.)
    -i ${CMAKE_SOURCE_DIR}/thirdparty
)

# Add suppressions file if it exists
if(EXISTS ${CPPCHECK_SUPPRESSIONS_FILE})
    list(APPEND CPPCHECK_CMD --suppressions-list=${CPPCHECK_SUPPRESSIONS_FILE})
    message(STATUS "[cppcheck] Using suppressions: ${CPPCHECK_SUPPRESSIONS_FILE}")
endif()

if(NOT TARGET cppcheck)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_CMD}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck on ${CMAKE_PROJECT_NAME}..."
        VERBATIM
    )
endif()

message(STATUS "[cppcheck] Target 'cppcheck' registered (${CMAKE_PROJECT_NAME})")
