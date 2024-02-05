

set(wxBUILD_SHARED OFF)

Set(FETCHCONTENT_QUITE FALSE)

message(STATUS "Fetching wxWidgets....")

FetchContent_Declare(
    wxWidgets
    GIT_PROGESS TRUE
    GIT_REPOSITORY https://github.com/wxWidgets/wxWidgets.git
    GIT_TAG v3.2.4
    GIT_SHALLOW ON
)

FetchContent_MakeAvailable(wxWidgets)