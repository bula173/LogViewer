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

message(STATUS "=== LDD Output for ${EXE_PATH} ===")
message(STATUS "${LDD_OUTPUT}")
message(STATUS "=== End LDD Output ===")

# Parse output and copy each DLL
string(REPLACE "\n" ";" LINES "${LDD_OUTPUT}")
set(DLL_LIST "")  # Initialize an empty list

foreach(LINE IN LISTS LINES)
    # Match "not found" DLLs first (higher priority)
    if(LINE MATCHES "([^ ]+\\.dll) => not found")
        # DLL not found - try to locate it in mingw64/bin
        string(REGEX REPLACE ".*[ \t]+([^ ]+\\.dll) => not found.*" "\\1" DLL_NAME "${LINE}")
        set(DLL_PATH "/mingw64/bin/${DLL_NAME}")
        list(APPEND DLL_LIST "${DLL_PATH}")
        message(STATUS "Found missing DLL (will search): ${DLL_PATH}")
    # Match lines like: "    libfoo.dll => /mingw64/bin/libfoo.dll (0x...)"
    elseif(LINE MATCHES "=>[ ]*(/.+\\.dll)")
        # Extract the DLL path using regex - capture the full path after =>
        string(REGEX REPLACE ".*=>[ ]*(/.+\\.dll).*" "\\1" DLL_PATH "${LINE}")
        # Only process paths that start with /mingw64/, /usr/, or /clangarm64/
        if(DLL_PATH MATCHES "^/(mingw64|usr|clangarm64)/")
            # If it's /clangarm64/, convert to /mingw64/
            if(DLL_PATH MATCHES "^/clangarm64/")
                string(REPLACE "/clangarm64/" "/mingw64/" DLL_PATH "${DLL_PATH}")
                message(STATUS "Found DLL (clangarm64 -> mingw64): ${DLL_PATH}")
            else()
                message(STATUS "Found DLL: ${DLL_PATH}")
            endif()
            list(APPEND DLL_LIST "${DLL_PATH}")
        endif()
    endif()
endforeach()

foreach(DLL_FULLPATH IN LISTS DLL_LIST)
    string(STRIP "${DLL_FULLPATH}" DLL_FULLPATH)
    # Convert MSYS2 path to Windows path if needed
    if(DLL_FULLPATH MATCHES "^/mingw64/")
        # Handle /mingw64/ paths
        string(REPLACE "/mingw64/" "C:/msys64/mingw64/" DLL_WIN_PATH "${DLL_FULLPATH}")
    elseif(DLL_FULLPATH MATCHES "^/usr/")
        # Handle /usr/ paths
        string(REPLACE "/usr/" "C:/msys64/usr/" DLL_WIN_PATH "${DLL_FULLPATH}")
    elseif(DLL_FULLPATH MATCHES "^/([a-zA-Z])/")
        # Handle /c/ style paths
        string(REGEX REPLACE "^/([a-zA-Z])/" "\\1:/" DLL_WIN_PATH "${DLL_FULLPATH}")
    else()
        # Already a Windows path or unknown format
        set(DLL_WIN_PATH "${DLL_FULLPATH}")
    endif()
    
    if(EXISTS "${DLL_WIN_PATH}")
        file(COPY "${DLL_WIN_PATH}" DESTINATION "${DEST_DIR}")
        get_filename_component(DLL_NAME "${DLL_WIN_PATH}" NAME)
        message(STATUS "Copied ${DLL_NAME} to ${DEST_DIR}")
    else()
        # If debug DLL (with 'd' suffix) not found, try release version
        string(REGEX REPLACE "d-([0-9]+)\\.dll$" "-\\1.dll" DLL_WIN_PATH_RELEASE "${DLL_WIN_PATH}")
        if(NOT "${DLL_WIN_PATH_RELEASE}" STREQUAL "${DLL_WIN_PATH}" AND EXISTS "${DLL_WIN_PATH_RELEASE}")
            file(COPY "${DLL_WIN_PATH_RELEASE}" DESTINATION "${DEST_DIR}")
            get_filename_component(DLL_NAME "${DLL_WIN_PATH_RELEASE}" NAME)
            message(WARNING "Debug DLL not found, copied release version: ${DLL_NAME} to ${DEST_DIR}")
        else()
            message(FATAL_ERROR "DLL not found: ${DLL_WIN_PATH} (original: ${DLL_FULLPATH})")
        endif()
    endif()
endforeach()