set(FETCHCONTENT_QUIET FALSE)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3 # or latest stable
)
FetchContent_MakeAvailable(nlohmann_json)
