
FetchContent_Declare(
    expat
    GIT_REPOSITORY https://github.com/libexpat/libexpat/
    GIT_TAG        R_2_7_3 # or latest stable
    SOURCE_SUBDIR  expat/
    SYSTEM
)

set(EXPAT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(EXPAT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(EXPAT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(EXPAT_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(EXPAT_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
# Disable debug postfix so we get libexpat-1.dll in both Debug and Release
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(expat)
set_target_properties(expat PROPERTIES DEBUG_POSTFIX "")

