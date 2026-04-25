# The updates module is header-only (UpdateInfo.hpp).
# Sources that use it (UpdateChecker, UpdateDialog) live in the Qt UI subtree.
# This CMakeLists.txt exists so add_subdirectory(updates) resolves cleanly.
target_include_directories(application_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)
