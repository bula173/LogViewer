# copy_dlls.cmake
set(EXE_PATH "${ARGV0}")
set(DEST_DIR "${ARGV1}")
set(GCC_PATH "${ARGV2}")

execute_process(
    COMMAND ldd "${EXE_PATH}"
    OUTPUT_VARIABLE LDD_OUTPUT
    RESULT_VARIABLE LDD_RESULT
    ERROR_VARIABLE LDD_ERROR
)

if(NOT LDD_RESULT EQUAL 0)
    message(FATAL_ERROR "ldd failed: ${LDD_ERROR}")
endif()

message(STATUS "Ldd output ${LDD_OUTPUT}")

# Parse output and copy each DLL
string(REPLACE "\n" ";" LINES "${LDD_OUTPUT}")
set(DLL_LIST "")  # Initialize an empty list

foreach(LINE IN LISTS LINES)
    # Match lines like: "    libfoo.dll => /mingw64/bin/libfoo.dll (0x...)"
    if(LINE MATCHES "=>[ ]*([^ ]*mingw64[^ ]*\\.dll)")
        # Extract the DLL path using regex
        string(REGEX REPLACE "(.*)=>[ ]*[^ ]*mingw64[^ ]*\\.dll.*" "\\1" DLL "${LINE}")
        list(APPEND DLL_LIST "${DLL}")
        message(STATUS "Found DLL: ${DLL}")

    endif()
endforeach()

get_filename_component(DLL_PATH ${GCC_PATH} PATH)

foreach(DLL IN LISTS DLL_LIST)
    string(STRIP "${DLL}" DLL)
    if(EXISTS "${DLL_PATH}/${DLL}")
        file(COPY "${DLL_PATH}/${DLL}" DESTINATION "${DEST_DIR}")
        message(STATUS "Copied ${DLL} to ${DEST_DIR}")
    else()
        message(WARNING "DLL${DLL} not found: ${DLL_PATH}/${DLL}")
    endif()
endforeach()