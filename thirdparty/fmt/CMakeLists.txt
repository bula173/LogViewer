set(FETCHCONTENT_QUIET FALSE)

set(FMT_USE_CONSTEVAL OFF CACHE BOOL "" FORCE)
set(FMT_CONSTEVAL "" CACHE STRING "" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 12.1.0
  GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(fmt)