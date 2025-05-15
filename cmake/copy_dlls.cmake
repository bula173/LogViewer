# copy_dlls.cmake
set(EXE_PATH "${ARGV0}")
set(DEST_DIR "${ARGV1}")

execute_process(
    COMMAND ldd "${EXE_PATH}"
    OUTPUT_VARIABLE LDD_OUTPUT
    RESULT_VARIABLE LDD_RESULT
    ERROR_VARIABLE LDD_ERROR
)

if(NOT LDD_RESULT EQUAL 0)
    message(FATAL_ERROR "ldd failed: ${LDD_ERROR}")
endif()

# Parse output and copy each DLL
string(REPLACE "\n" ";" LINES "${LDD_OUTPUT}")
foreach(LINE IN LISTS LINES)
    if(LINE MATCHES "=> ([^ ]+\\.dll)")
        string(REGEX REPLACE ".*=> ([^ ]+\\.dll).*" "\\1" DLL_PATH "${LINE}")
        if(EXISTS "${DLL_PATH}")
            file(COPY "${DLL_PATH}" DESTINATION "${DEST_DIR}")
            message(STATUS "Copied ${DLL_PATH}")
        else()
            message(WARNING "DLL not found: ${DLL_PATH}")
        endif()
    endif()
endforeach()
