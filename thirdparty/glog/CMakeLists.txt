set(wxBUILD_SHARED OFF)

set(FETCHCONTENT_QUIET FALSE)

message(STATUS "Fetching glog....")

FetchContent_Declare(
    glog
    GIT_PROGRESS TRUE
    GIT_REPOSITORY https://github.com/google/glog.git
    GIT_SHALLOW ON
    GIT_TAG master
)

# Check if glog has already been downloaded
FetchContent_GetProperties(glog)
if(NOT glog_POPULATED)
    message(STATUS "Fetching glog....")
    FetchContent_Populate(glog)
    add_subdirectory(${glog_SOURCE_DIR} ${glog_BINARY_DIR})
endif()

set(GLOG_INCLUDE_DIR "${glog_SOURCE_DIR}/src" CACHE PATH "Path to the glog include directory")

# Check if GLOG_INCLUDE_DIR is set
if(NOT GLOG_INCLUDE_DIR)
    message(FATAL_ERROR "glog include directory not found! Fetching or configuring glog failed.")
endif()

# Make glog available
FetchContent_MakeAvailable(glog)


