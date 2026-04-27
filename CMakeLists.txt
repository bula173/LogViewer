cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(LogViewer VERSION 1.2.2 DESCRIPTION "A modern log viewer with AI-assisted analysis" LANGUAGES CXX C)

cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# MinGW static runtime — opt-in to avoid ABI conflicts on mixed environments
# ---------------------------------------------------------------------------
if(MINGW)
    option(USE_STATIC_MINGW_LIBS "Link libstdc++ and libgcc statically on MinGW" OFF)
    if(USE_STATIC_MINGW_LIBS)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libstdc++ -static-libgcc")
    endif()
endif()

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(BUILD_SDK "Build and install the plugin SDK" ON)

# Quality-tool options — match variable names used in CMakePresets.json
option(ENABLE_CPPCHECK   "Enable cppcheck static analysis target"  OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy analysis target"       OFF)
option(CLANG_TIDY_FIX    "Apply clang-tidy fixes automatically"    OFF)

# Sanitizer — preset sets LOGVIEWER_SANITIZER=Address|Thread|Memory|None
# The cmake/EnableSanitizers.cmake module declares this as a CACHE STRING
# with allowed values. Default to None so plain cmake -B build/ is safe.
set(LOGVIEWER_SANITIZER "None" CACHE STRING
    "Sanitizer: Address | Thread | Memory | UndefinedBehavior | None")

# ---------------------------------------------------------------------------
# Load cmake/ helper modules (order matters: warnings before subdirectories
# so the function is available when targets are created in src/)
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(cmake/Warnings.cmake)
include(cmake/EnableSanitizers.cmake)
include(cmake/Cppcheck.cmake)
include(cmake/ClangTidy.cmake)
include(cmake/ClangFormat.cmake)

# ---------------------------------------------------------------------------
# Subproject detection — when used via FetchContent only build the SDK
# ---------------------------------------------------------------------------
if(NOT CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    message(STATUS "LogViewer: Building as subproject (SDK only)")
    set(LOGVIEWER_SUBPROJECT TRUE)
else()
    message(STATUS "LogViewer: Building as standalone project")
    set(LOGVIEWER_SUBPROJECT FALSE)
endif()

# ---------------------------------------------------------------------------
# Source tree
# ---------------------------------------------------------------------------
add_subdirectory(thirdparty)
add_subdirectory(src)

if(NOT LOGVIEWER_SUBPROJECT)
    add_subdirectory(tests)
    add_subdirectory(etc)
    add_subdirectory(plugins)
    add_subdirectory(examples)
    add_subdirectory(packaging)
endif()

# ---------------------------------------------------------------------------
# Apply sanitizers to all instrumented targets after they are defined
# Targets listed here are the ones whose runtime behaviour we want to check.
# ---------------------------------------------------------------------------
if(NOT LOGVIEWER_SUBPROJECT)
    logviewer_enable_sanitizers(application_core LogViewer LogViewer_tests)
endif()

# ---------------------------------------------------------------------------
# Aggregate quality-tools target — depends on all individual tool targets
# cmake --build --preset macos-quality-tools
# cmake --build --preset linux-quality-tools
# ---------------------------------------------------------------------------
if(NOT TARGET run_tools)
    add_custom_target(run_tools
        COMMENT "Running all code quality tools (cppcheck, tidy, format-check)"
    )
    add_dependencies(run_tools cppcheck tidy format-check)
endif()
