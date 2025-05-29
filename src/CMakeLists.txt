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

add_subdirectory(application)
add_subdirectory(main)
