# copy_dlls.cmake
set(EXE_PATH "${ARGV0}")
set(DEST_DIR "${ARGV1}")
set(GCC_PATH "${ARGV2}")

# Detect MSYS2 root from compiler path (e.g., C:/msys64/mingw64/bin/clang.exe or C:/msys64/clangarm64/bin/clang.exe)
get_filename_component(COMPILER_DIR "${GCC_PATH}" DIRECTORY)  # Gets the /bin directory
get_filename_component(MINGW_ROOT "${COMPILER_DIR}" DIRECTORY)  # Gets the toolchain directory (mingw64, clangarm64, etc.)
get_filename_component(MSYS2_ROOT "${MINGW_ROOT}" DIRECTORY)    # Gets the /msys64 directory

message(STATUS "Detected MSYS2 root: ${MSYS2_ROOT}")
message(STATUS "Detected toolchain root: ${MINGW_ROOT}")


# Print file status for diagnostics (Windows only)
if(WIN32)
  # Convert Windows path to MSYS2 path
  execute_process(
    COMMAND bash -c "cygpath -u '${ARGV0}'"
    OUTPUT_VARIABLE msys2_exe_path
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  message(STATUS "MSYS2 exe path: ${msys2_exe_path}")

  execute_process(COMMAND bash -c "ls -l '${msys2_exe_path}'" OUTPUT_VARIABLE file_status ERROR_VARIABLE file_status_err)
  message(STATUS "File status: ${file_status}")
  if(NOT "${file_status_err}" STREQUAL "")
    message(WARNING "ls error: ${file_status_err}")
  endif()

  # Ensure file is readable and executable using MSYS2 chmod
  execute_process(COMMAND bash -c "chmod +rx '${msys2_exe_path}'" RESULT_VARIABLE chmod_result)
  if(NOT chmod_result EQUAL 0)
    message(WARNING "chmod failed on ${msys2_exe_path}, result: ${chmod_result}")
  endif()

  # Sleep for 1 second to avoid file locking issues
  execute_process(COMMAND bash -c "sleep 1")

  # Run ldd via MSYS2 bash with MSYS2 path
  execute_process(
    COMMAND bash -c "ldd '${msys2_exe_path}'"
    RESULT_VARIABLE ldd_result
    OUTPUT_VARIABLE ldd_output
    ERROR_VARIABLE ldd_error
  )
  message(STATUS "ldd output: ${ldd_output}")
  if(NOT ldd_result EQUAL 0)
    message(WARNING "ldd failed (skipping DLL copy): ${ldd_error}")
    return()
  endif()
else()
  # Now run ldd
  execute_process(
    COMMAND ldd "${ARGV0}"
    RESULT_VARIABLE ldd_result
    OUTPUT_VARIABLE ldd_output
    ERROR_VARIABLE ldd_error
  )

  if(NOT ldd_result EQUAL 0)
    message(WARNING "ldd failed (skipping DLL copy): ${ldd_error}")
    return()
  endif()

  message(STATUS "=== LDD Output for ${EXE_PATH} ===")
  message(STATUS "${ldd_output}")
  message(STATUS "=== End LDD Output ===")
endif()

# Parse output and copy each DLL
string(REPLACE "\n" ";" LINES "${ldd_output}")
set(DLL_LIST "")  # Initialize an empty list

foreach(LINE IN LISTS LINES)
    # Match "not found" DLLs first (higher priority)
    if(LINE MATCHES "([^ ]+\\.dll) => not found")
        # DLL not found - try to locate it in current toolchain/bin
        string(REGEX REPLACE ".*[ \t]+([^ ]+\\.dll) => not found.*" "\\1" DLL_NAME "${LINE}")
        # Use the detected MINGW_ROOT which should be the actual toolchain directory
        get_filename_component(TOOLCHAIN_NAME "${MINGW_ROOT}" NAME)
        set(DLL_PATH "/${TOOLCHAIN_NAME}/bin/${DLL_NAME}")
        list(APPEND DLL_LIST "${DLL_PATH}")
        message(STATUS "Found missing DLL (will search): ${DLL_PATH}")
    # Match lines like: "    libfoo.dll => /mingw64/bin/libfoo.dll (0x...)" or "    libfoo.dll => /clangarm64/bin/libfoo.dll (0x...)"
    elseif(LINE MATCHES "=>[ ]*(/.+\\.dll)")
        # Extract the DLL path using regex - capture the full path after =>
        string(REGEX REPLACE ".*=>[ ]*(/.+\\.dll).*" "\\1" DLL_PATH "${LINE}")
        # Only process paths that start with /mingw64/, /usr/, or /clangarm64/
        if(DLL_PATH MATCHES "^/(mingw64|usr|clangarm64)/")
            message(STATUS "Found DLL: ${DLL_PATH}")
            list(APPEND DLL_LIST "${DLL_PATH}")
        endif()
    endif()
endforeach()

  list(LENGTH DLL_LIST DLL_COUNT)
  if(DLL_COUNT EQUAL 0)
    message(WARNING "ldd output did not list any DLLs for ${EXE_PATH}; skipping copy")
    return()
  endif()

foreach(DLL_ITEM IN LISTS DLL_LIST)
    # DLL_ITEM is the path from ldd
    set(DLL_FULLPATH "${DLL_ITEM}")
    
    string(STRIP "${DLL_FULLPATH}" DLL_FULLPATH)
    # Convert MSYS2 path to Windows path if needed
    if(DLL_FULLPATH MATCHES "^/(mingw64|clangarm64)/")
        # Handle /mingw64/ or /clangarm64/ paths - use detected MSYS2_ROOT
        string(REPLACE "/mingw64/" "${MSYS2_ROOT}/mingw64/" DLL_WIN_PATH "${DLL_FULLPATH}")
        string(REPLACE "/clangarm64/" "${MSYS2_ROOT}/clangarm64/" DLL_WIN_PATH "${DLL_WIN_PATH}")
        # Normalize path separators to forward slashes
        file(TO_CMAKE_PATH "${DLL_WIN_PATH}" DLL_WIN_PATH)
    elseif(DLL_FULLPATH MATCHES "^/usr/")
        # Handle /usr/ paths - use detected MSYS2_ROOT
        string(REPLACE "/usr/" "${MSYS2_ROOT}/usr/" DLL_WIN_PATH "${DLL_FULLPATH}")
        # Normalize path separators to forward slashes
        file(TO_CMAKE_PATH "${DLL_WIN_PATH}" DLL_WIN_PATH)
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
            # Skip system DLLs or missing ones
            message(WARNING "DLL not found, skipping: ${DLL_WIN_PATH} (original: ${DLL_FULLPATH})")
        endif()
    endif()
endforeach()