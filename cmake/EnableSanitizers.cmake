# cmake/EnableSanitizers.cmake
#
# Reads the LOGVIEWER_SANITIZER cache variable (set by CMakePresets.json) and
# applies the requested sanitizer flags to a supplied list of targets.
#
# Usage (called from root CMakeLists.txt after all targets are defined):
#
#   include(cmake/EnableSanitizers.cmake)
#   logviewer_enable_sanitizers(application_core LogViewer)
#
# Supported LOGVIEWER_SANITIZER values:
#   Address            — ASan + UBSan  (default for debug presets)
#   Thread             — TSan
#   Memory             — MSan (Clang only)
#   UndefinedBehavior  — UBSan only
#   None               — disabled (Windows presets, release presets)
#   <empty>            — treated as None

set(LOGVIEWER_SANITIZER "None" CACHE STRING
    "Sanitizer to enable: Address | Thread | Memory | UndefinedBehavior | None")
set_property(CACHE LOGVIEWER_SANITIZER PROPERTY STRINGS
    Address Thread Memory UndefinedBehavior None)

# ---------------------------------------------------------------------------
# Internal helper — applies flags to a single target
# ---------------------------------------------------------------------------
function(_logviewer_apply_sanitizer_to_target target flags)
    if(TARGET ${target})
        target_compile_options(${target} PRIVATE ${flags})
        target_link_options(${target} PRIVATE ${flags})
        message(STATUS "  [sanitizer] ${target}: ${flags}")
    else()
        message(WARNING "[sanitizer] Target '${target}' not found — skipping")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Public function — call with the list of targets to instrument
# ---------------------------------------------------------------------------
function(logviewer_enable_sanitizers)
    set(targets ${ARGV})

    # Sanitizers are not supported on Windows with MSYS2/MinGW
    if(WIN32)
        message(STATUS "[sanitizer] Sanitizers disabled on Windows")
        return()
    endif()

    if(NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
        message(STATUS "[sanitizer] Sanitizers require Clang or GCC — skipping (${CMAKE_CXX_COMPILER_ID})")
        return()
    endif()

    if(LOGVIEWER_SANITIZER STREQUAL "None" OR LOGVIEWER_SANITIZER STREQUAL "")
        message(STATUS "[sanitizer] Disabled (LOGVIEWER_SANITIZER=None)")
        return()
    endif()

    # Map friendly names to compiler flags
    if(LOGVIEWER_SANITIZER STREQUAL "Address")
        set(san_flags -fsanitize=address,undefined -fno-omit-frame-pointer)
        message(STATUS "[sanitizer] Enabling AddressSanitizer + UBSan on: ${targets}")

    elseif(LOGVIEWER_SANITIZER STREQUAL "Thread")
        set(san_flags -fsanitize=thread -fno-omit-frame-pointer)
        # TSan is incompatible with ASan — warn if accidentally combined
        message(STATUS "[sanitizer] Enabling ThreadSanitizer on: ${targets}")

    elseif(LOGVIEWER_SANITIZER STREQUAL "Memory")
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(WARNING "[sanitizer] MemorySanitizer requires Clang — skipping")
            return()
        endif()
        set(san_flags -fsanitize=memory -fno-omit-frame-pointer -fsanitize-memory-track-origins)
        message(STATUS "[sanitizer] Enabling MemorySanitizer on: ${targets}")

    elseif(LOGVIEWER_SANITIZER STREQUAL "UndefinedBehavior")
        set(san_flags -fsanitize=undefined -fno-omit-frame-pointer)
        message(STATUS "[sanitizer] Enabling UndefinedBehaviorSanitizer on: ${targets}")

    else()
        message(WARNING "[sanitizer] Unknown LOGVIEWER_SANITIZER value: '${LOGVIEWER_SANITIZER}'. "
                        "Valid values: Address, Thread, Memory, UndefinedBehavior, None")
        return()
    endif()

    foreach(target IN LISTS targets)
        _logviewer_apply_sanitizer_to_target(${target} "${san_flags}")
    endforeach()
endfunction()
