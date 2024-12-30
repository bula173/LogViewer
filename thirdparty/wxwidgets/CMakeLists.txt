set(wxBUILD_SHARED OFF)

set(FETCHCONTENT_QUIET FALSE)

message(STATUS "Checking for system wxWidgets....")

# Try to find system wxWidgets
find_package(wxWidgets COMPONENTS core base REQUIRED)

if(wxWidgets_FOUND)
    message(STATUS "Using system wxWidgets")
    include(${wxWidgets_USE_FILE})
else()
    message(STATUS "System wxWidgets not found, fetching wxWidgets....")

    FetchContent_Declare(
        wxWidgets
        GIT_PROGRESS TRUE
        GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
        GIT_TAG v3.2.6
        GIT_SHALLOW ON
    )

    # Check if wxWidgets has already been downloaded
    FetchContent_GetProperties(wxWidgets)

    if(NOT wxWidgets_POPULATED)
        message(STATUS "Fetching wxWidgets....")
        FetchContent_Populate(wxWidgets)
    endif()
    # Make glog available
    FetchContent_MakeAvailable(wxWidgets)
endif()
