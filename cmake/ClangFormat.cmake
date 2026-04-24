# cmake/ClangFormat.cmake
#
# Creates two targets:
#   format        — reformat all source files in-place using clang-format
#   format-check  — verify formatting without modifying files (for CI)
#
# Both targets operate on all .cpp/.hpp files under src/, excluding thirdparty.
#
# Usage:
#   include(cmake/ClangFormat.cmake)         # in root CMakeLists.txt
#   cmake --build --preset linux-format      # reformat in-place
#   cmake --build <dir> --target format-check  # CI check

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format clang-format-18 clang-format-17 clang-format-16)

# ---------------------------------------------------------------------------
# Collect all project source files (exclude thirdparty & build dir)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ALL_FORMAT_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_SOURCE_DIR}/tests/*.hpp"
    "${CMAKE_SOURCE_DIR}/plugins/*.cpp"
    "${CMAKE_SOURCE_DIR}/plugins/*.hpp"
    "${CMAKE_SOURCE_DIR}/examples/*.cpp"
    "${CMAKE_SOURCE_DIR}/examples/*.hpp"
)

if(NOT CLANG_FORMAT_EXECUTABLE)
    message(STATUS "[clang-format] Not found — format/format-check targets will be no-ops. "
                   "Install clang-format or set CLANG_FORMAT_EXECUTABLE.")

    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found — skipping format"
        COMMENT "clang-format: not found"
    )
    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found — skipping format-check"
        COMMENT "clang-format: not found"
    )
    return()
endif()

message(STATUS "[clang-format] Found: ${CLANG_FORMAT_EXECUTABLE}")

# ---------------------------------------------------------------------------
# format — reformat files in-place
# ---------------------------------------------------------------------------
add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXECUTABLE}
        --style=file          # Use .clang-format in source tree
        -i                    # In-place modification
        ${ALL_FORMAT_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Reformatting sources with clang-format..."
    VERBATIM
)

# ---------------------------------------------------------------------------
# format-check — dry-run check used in CI (exits non-zero on violations)
# ---------------------------------------------------------------------------
add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXECUTABLE}
        --style=file
        --dry-run             # Don't modify files
        --Werror              # Exit non-zero on any formatting violation
        ${ALL_FORMAT_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking formatting with clang-format (CI mode)..."
    VERBATIM
)

message(STATUS "[clang-format] Targets 'format' and 'format-check' registered")
