set(FETCHCONTENT_QUIET FALSE)

# Fetch gflags
FetchContent_Declare(
  gflags
  GIT_PROGRESS TRUE
  GIT_REPOSITORY https://github.com/gflags/gflags.git
  GIT_TAG        v2.2.2
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Make gflags available
FetchContent_MakeAvailable(gflags)
