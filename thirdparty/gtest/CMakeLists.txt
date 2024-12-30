enable_testing()

set(FETCHCONTENT_QUIET FALSE)

# UNIT TEST
FetchContent_Declare(
  googletest
  GIT_PROGRESS TRUE
  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Check if googletest has already been downloaded
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  message(STATUS "Fetching googletest....")
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

FetchContent_MakeAvailable(googletest)
