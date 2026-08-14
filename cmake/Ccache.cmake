# cmake/Ccache.cmake
#
# Auto-detects ccache and wires it in as the C/C++ compiler launcher if found.
# Presets no longer hardcode CMAKE_C(XX)_COMPILER_LAUNCHER=ccache directly —
# that fails configure on any machine without ccache on PATH. This module
# degrades gracefully instead, matching the pattern used by ClangTidy.cmake /
# Cppcheck.cmake for other optional tools.
#
# A preset (or the command line) can still force a specific launcher by
# setting CMAKE_C_COMPILER_LAUNCHER / CMAKE_CXX_COMPILER_LAUNCHER explicitly
# before this module runs — that value is left untouched.

if(CMAKE_C_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER)
    return()
endif()

find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
    message(STATUS "[ccache] Found: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}" CACHE STRING "")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE STRING "")
else()
    message(STATUS "[ccache] Not found — building without compiler cache")
endif()
