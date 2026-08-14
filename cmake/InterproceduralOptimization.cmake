# cmake/InterproceduralOptimization.cmake
#
# Enables link-time optimization (LTO) for Release builds of the project's
# own primary target only. Deliberately NOT applied globally via the
# CMAKE_INTERPROCEDURAL_OPTIMIZATION variable — that would also LTO-build
# every thirdparty/ dependency (llama.cpp in particular is large and not
# validated against LTO here), which would significantly slow down Release
# builds for no measured benefit.
#
# Usage: call logviewer_enable_lto(<target> [<target> ...]) after the
# targets are defined.

include(CheckIPOSupported)
check_ipo_supported(RESULT LOGVIEWER_IPO_SUPPORTED OUTPUT LOGVIEWER_IPO_ERROR)

function(logviewer_enable_lto)
    if(NOT LOGVIEWER_IPO_SUPPORTED)
        message(STATUS "[LTO] Not supported by this toolchain: ${LOGVIEWER_IPO_ERROR}")
        return()
    endif()

    foreach(target IN LISTS ARGN)
        if(TARGET ${target})
            set_target_properties(${target} PROPERTIES
                INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
            )
        endif()
    endforeach()

    message(STATUS "[LTO] Enabled for Release builds: ${ARGN}")
endfunction()
