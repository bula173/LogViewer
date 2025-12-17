# Set standard CMake output directories - build outputs go to build/<preset>/bin and lib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# For multi-config generators (Visual Studio, Xcode)
foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_BINARY_DIR}/lib)
endforeach()

# Set NDEBUG for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_definitions("NDEBUG")
endif()

# Enable helpful warnings (C++ standard set globally in root CMakeLists.txt)
if(MSVC)
    add_compile_options(/W4 /permissive-)
    # Optionally treat warnings as errors on MSVC
    # add_compile_options(/WX)
else()
    add_compile_options(
        -Wall -Wextra -Wshadow -Wnon-virtual-dtor
        -Wold-style-cast -Wcast-align -Wunused
        -Woverloaded-virtual -Wpedantic -Wconversion
        -Wsign-conversion -Wnull-dereference
        -Wdouble-promotion -Wformat=2
    )
    # Note: -Werror disabled due to existing conversion warnings
    # Enable with: add_compile_options(-Werror) after fixing:
    #   - version.cpp: int/long long conversions
    #   - EventsContainer.cpp: signed/unsigned conversions
    #   - FilterManager.cpp: signed/unsigned conversions
endif()

# Include cleanCppExtensions for convenience targets (format, cppcheck, tidy, etc.)
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/cleanCppExtensions.cmake")
    message(STATUS "Using cleanCppExtensions.cmake from ${CMAKE_SOURCE_DIR}/cmake")
    include("${CMAKE_SOURCE_DIR}/cmake/cleanCppExtensions.cmake")
    # Create convenience targets for formatting and static analysis
    addMiscTargets()
else()
    message(ERROR "cleanCppExtensions.cmake not found at ${CMAKE_SOURCE_DIR}/cmake, run and debug targets not available")
endif()

# Include sanitizers (but apply to targets in src/main/CMakeLists.txt)
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/sanitizers.cmake")
    message(STATUS "Using sanitizers.cmake from ${CMAKE_SOURCE_DIR}/cmake")
    include("${CMAKE_SOURCE_DIR}/cmake/sanitizers.cmake")
else()
    message(WARNING "sanitizers.cmake not found at ${CMAKE_SOURCE_DIR}/cmake, sanitizers not available")
endif()

# Custom target to run all useful tools (format, cppcheck, etc.)
# Note: cppcheck, format, and tidy targets are provided by cleanCppExtensions.cmake via addMiscTargets()
add_custom_target(run_tools
    COMMAND ${CMAKE_COMMAND} -E echo "=== Running Code Quality Tools ==="
    DEPENDS format cppcheck
    COMMENT "Running code formatting and static analysis tools"
)

#-------------------------------------------------------------------------------

if(MINGW)
    message(STATUS "MinGW detected")
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/dist/lib/"
        DESTINATION .
        COMPONENT common
        FILES_MATCHING PATTERN "*.dll")
endif()

#------------------------------------------------------------------------------

add_subdirectory(application)
add_subdirectory(main)
add_subdirectory(plugins)
