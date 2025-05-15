message(STATUS "Building LogViewer::application")
message (STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE})

# Enable exporting compile commands
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE APP_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(application STATIC ${APP_SRCS})

if(WX_LOCAL_BUILD)
    message(STATUS "wxWidgets Not Found in system!")
    # Use wx-config after wxWidgets is built
    add_dependencies(application BuildWxWidgets)

    set(WX_CONFIG_EXECUTABLE "$ENV{WX_CONFIG}")

    # Generate wxWidgets flags using wx-config
    execute_process(
        COMMAND ${WX_CONFIG_EXECUTABLE} --libs
        OUTPUT_VARIABLE wxWidgets_LIBRARIES
        RESULT_VARIABLE RET
        ERROR_QUIET
    )

    if(RET EQUAL 0)
        string(STRIP "${wxWidgets_LIBRARIES}" wxWidgets_LIBRARIES)
        separate_arguments(wxWidgets_LIBRARIES)
        string(REPLACE "-framework;" "-framework "
        wxWidgets_LIBRARIES "${wxWidgets_LIBRARIES}")
        string(REPLACE "-weak_framework;" "-weak_framework "
        wxWidgets_LIBRARIES "${wxWidgets_LIBRARIES}")
        string(REPLACE "-arch;" "-arch "
        wxWidgets_LIBRARIES "${wxWidgets_LIBRARIES}")
        string(REPLACE "-isysroot;" "-isysroot "
        wxWidgets_LIBRARIES "${wxWidgets_LIBRARIES}")

        # extract linkdirs (-L) for rpath (i.e., LINK_DIRECTORIES)
        string(REGEX MATCHALL "-L[^;]+"
        wxWidgets_LIBRARY_DIRS "${wxWidgets_LIBRARIES}")
        string(REGEX REPLACE "-L([^;]+)" "\\1"
        wxWidgets_LIBRARY_DIRS "${wxWidgets_LIBRARY_DIRS}")
    endif()


    message(STATUS "wxWidgets_LIBRARIES: ${wxWidgets_LIBRARIES}")

    execute_process(
        COMMAND ${WX_CONFIG_EXECUTABLE} --cxxflags
        OUTPUT_VARIABLE wxWidgets_CXX_FLAGS
        RESULT_VARIABLE RET
        ERROR_QUIET
    )

    if(RET EQUAL 0)
        string(STRIP "${wxWidgets_CXX_FLAGS}" wxWidgets_CXX_FLAGS)
        separate_arguments(wxWidgets_CXX_FLAGS_LIST NATIVE_COMMAND "${wxWidgets_CXX_FLAGS}")

        # parse definitions and include dirs from cxxflags
        #   drop the -D and -I prefixes
        set(wxWidgets_CXX_FLAGS)
        foreach(arg IN LISTS wxWidgets_CXX_FLAGS_LIST)
            if("${arg}" MATCHES "^-I(.*)$")
                # include directory
                list(APPEND wxWidgets_INCLUDE_DIRS "${CMAKE_MATCH_1}")
            elseif("${arg}" MATCHES "^-D(.*)$")
                # compile definition
                list(APPEND wxWidgets_DEFINITIONS "${CMAKE_MATCH_1}")
            else()
                list(APPEND wxWidgets_CXX_FLAGS "${arg}")
            endif()
        endforeach()
    endif()

    target_compile_definitions(application PUBLIC ${wxWidgets_DEFINITIONS})

else()
    find_package(wxWidgets REQUIRED COMPONENTS core base xml)

    # If system wxWidgets is found, just use it directly
    message(STATUS "wxWidgets Found in system!")
    # Use the static library configuration (if available)
    set(wxWidgets_USE_STATIC_LIBS ON)
    include(${wxWidgets_USE_FILE})
    target_compile_definitions(application PUBLIC ${wxWidgets_DEFINITIONS})
    # Make wxWidgets variables global
    #set(wxWidgets_LIBRARIES "${wxWidgets_LIBRARIES}" CACHE STRING "wxWidgets libraries" FORCE)

endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")

else()
    message(STATUS "clang-tidy not found.")
endif()

add_custom_target(clang-tidy
    COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} ${APP_SRCS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy"
)

target_link_libraries(application INTERFACE ${wxWidgets_LIBRARIES})
target_include_directories(application PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${wxWidgets_INCLUDE_DIRS})

#-------------------------------------------------------------------------------

if(MINGW)
    message(STATUS "MinGW detected - application")
# Install required DLLs explicitly from MSYS2's mingw64/bin
# You can add more as needed
set(MSYS_DLL_DIR "/msys64/mingw64/bin")
# Collect all wxWidgets DLLs
file(GLOB WX_DLLS "${MSYS_DLL_DIR}/wx*.dll")

install(FILES
    ${WX_DLLS}
    ${MSYS_DLL_DIR}/libpng16-16.dll
    ${MSYS_DLL_DIR}/zlib1.dll
    ${MSYS_DLL_DIR}/libjpeg-8.dll
    ${MSYS_DLL_DIR}/libexpat-1.dll
    DESTINATION bin)
endif(MINGW)




