# Provides a robust, mutually-exclusive sanitizer selection.

# Only proceed if using a compatible compiler
if (NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
    return()
endif()

# Define the list of available sanitizers.
# MemorySanitizer is not supported on macOS.
if (APPLE)
    set(LOGVIEWER_SANITIZER_CHOICES "None" "Address" "Thread" "Undefined")
else()
    set(LOGVIEWER_SANITIZER_CHOICES "None" "Address" "Thread" "Undefined" "Memory")
endif()

# Create a user-configurable option to select ONE sanitizer from the list.
set(LOGVIEWER_SANITIZER "None" CACHE STRING "Select a sanitizer to enable.")
set_property(CACHE LOGVIEWER_SANITIZER PROPERTY STRINGS ${LOGVIEWER_SANITIZER_CHOICES})

# If no sanitizer is selected, do nothing.
if (LOGVIEWER_SANITIZER STREQUAL "None")
    message(STATUS "Sanitizer: None")
    return()
endif()

message(STATUS "Sanitizer: ${LOGVIEWER_SANITIZER} enabled")

# Set the compiler flags based on the user's single, valid choice.
if (LOGVIEWER_SANITIZER STREQUAL "Address")
    set(SANITIZER_FLAGS "-fsanitize=address")
elseif (LOGVIEWER_SANITIZER STREQUAL "Thread")
    set(SANITIZER_FLAGS "-fsanitize=thread")
elseif (LOGVIEWER_SANITIZER STREQUAL "Undefined")
    set(SANITIZER_FLAGS "-fsanitize=undefined")
elseif (LOGVIEWER_SANITIZER STREQUAL "Memory")
    set(SANITIZER_FLAGS "-fsanitize=memory")
endif()

# Add common flags required by most sanitizers for better stack traces.
list(APPEND SANITIZER_FLAGS "-fno-omit-frame-pointer" "-g")

# Apply the flags to the specified target.
# This function will be called from the main CMakeLists.txt
function(target_enable_sanitizer TARGET_NAME)
    target_compile_options(${TARGET_NAME} PRIVATE ${SANITIZER_FLAGS})
    target_link_options(${TARGET_NAME} PRIVATE ${SANITIZER_FLAGS})
endfunction()

