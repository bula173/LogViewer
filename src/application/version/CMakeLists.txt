# ============================================================
# Version / resources
# ============================================================

configure_file(
    Version.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/Version.cpp
)

target_sources(application_core
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/Version.cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/Version.hpp>
        $<INSTALL_INTERFACE:Version.hpp>
)



