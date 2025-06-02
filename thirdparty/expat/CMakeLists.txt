set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
    expat
    GIT_REPOSITORY https://github.com/libexpat/libexpat/
    GIT_TAG        R_2_7_1 # or latest stable
    SOURCE_SUBDIR  expat/
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(expat)
