if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/Debug)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/Release)
    add_compile_definitions("NDEBUG")
else()
    message("----Unknown build type-------")
endif()


# Enable helpfull warnings and C++20 for all files
if(MSVC)
    add_compile_options(/std:c++20 /W3 /WX )
else()
    add_compile_options(-std=c++20 -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wunused -pedantic)
endif()

# Adds convenience methods, see cmake/cleanCppExtensions.cmake

# Using macro from cmake/cleanCpppExtensions.cpp included on the top
# Provides ${PROJECT_NAME}-run and ${PROJECT_NAME}-dbg
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/cleanCppExtensions.cmake")
    message(STATUS "Using cleanCppExtensions.cmake from ${CMAKE_SOURCE_DIR}/cmake")
    include("${CMAKE_SOURCE_DIR}/cmake/cleanCppExtensions.cmake")
else()
    message(WARNING "cleanCppExtensions.cmake not found at ${CMAKE_SOURCE_DIR}/cmake, run and debug targets not available")
endif()


if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/sanitizers.cmake")
    message(STATUS "Using sanitizers.cmake from ${CMAKE_SOURCE_DIR}/cmake")
    include("${CMAKE_SOURCE_DIR}/cmake/sanitizers.cmake")
else()
    message(WARNING "sanitizers.cmake not found at ${CMAKE_SOURCE_DIR}/cmake, sanitizers not available")
endif()

#-------------------------------------------------------------------------------

if(MINGW)
    message(STATUS "MinGW detected")

    install(DIRECTORY "${PROJECT_SOURCE_DIR}/dist/lib/"
    DESTINATION .
    FILES_MATCHING PATTERN "*.dll"
)
endif(MINGW)

#------------------------------------------------------------------------------
# Other MISC targets

# Adds misc targets: format, cppcheck, tidy, see cmake/cleanCppExtensions.cmake
addMiscTargets()

add_subdirectory(application)
add_subdirectory(main)
