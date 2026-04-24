# cmake/ClangTidy.cmake
#
# Creates a `tidy` custom target that runs clang-tidy on the project sources.
# Also optionally integrates clang-tidy into the build via CMAKE_CXX_CLANG_TIDY
# (runs during compilation rather than as a separate step).
#
# Controlled by:
#   ENABLE_CLANG_TIDY  BOOL  — enable the tidy target and/or compile integration
#   CLANG_TIDY_FIX     BOOL  — pass --fix to clang-tidy (auto-apply suggestions)
#
# Usage:
#   include(cmake/ClangTidy.cmake)        # in root CMakeLists.txt
#   cmake --build --preset linux-clang-tidy   # run standalone
#   cmake --build --preset macos-clang-tidy

option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
option(CLANG_TIDY_FIX    "Apply clang-tidy fixes automatically" OFF)

if(NOT ENABLE_CLANG_TIDY)
    # Create a stub target so presets that reference it don't fail at configure time
    if(NOT TARGET tidy)
        add_custom_target(tidy
            COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy is disabled (ENABLE_CLANG_TIDY=OFF)"
            COMMENT "clang-tidy: disabled"
        )
    endif()
    return()
endif()

find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy clang-tidy-18 clang-tidy-17 clang-tidy-16)

if(NOT CLANG_TIDY_EXECUTABLE)
    message(WARNING "[clang-tidy] Executable not found — tidy target will be a no-op. "
                    "Install clang-tidy or set CLANG_TIDY_EXECUTABLE.")
    if(NOT TARGET tidy)
        add_custom_target(tidy
            COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy not found"
            COMMENT "clang-tidy: not found"
        )
    endif()
    return()
endif()

message(STATUS "[clang-tidy] Found: ${CLANG_TIDY_EXECUTABLE}")

# ---------------------------------------------------------------------------
# Collect all project source files (exclude thirdparty & build directory)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ALL_SOURCE_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.hpp"
)

# Build the clang-tidy command
set(CLANG_TIDY_CMD ${CLANG_TIDY_EXECUTABLE})

if(CLANG_TIDY_FIX)
    list(APPEND CLANG_TIDY_CMD --fix --fix-errors)
    message(STATUS "[clang-tidy] Auto-fix mode enabled")
endif()

# Use the project .clang-tidy config; --warnings-as-errors applied there
list(APPEND CLANG_TIDY_CMD
    -p "${CMAKE_BINARY_DIR}"    # compile_commands.json location
    --quiet
)

# ---------------------------------------------------------------------------
# Standalone `tidy` target — run with: cmake --build <dir> --target tidy
# ---------------------------------------------------------------------------
if(NOT TARGET tidy)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_CMD} ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on ${CMAKE_PROJECT_NAME} sources..."
        VERBATIM
    )
endif()

message(STATUS "[clang-tidy] Target 'tidy' registered (${CMAKE_PROJECT_NAME})")
