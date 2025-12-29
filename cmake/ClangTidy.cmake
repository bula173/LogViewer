# Add Clang-Tidy support
find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
if (CLANG_TIDY_EXECUTABLE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE};--warnings-as-errors=*;--quiet")
    message(STATUS "Clang-Tidy found: ${CLANG_TIDY_EXECUTABLE}")
else()
    message(WARNING "Clang-Tidy not found. Skipping Clang-Tidy checks.")
endif()