# cmake/Warnings.cmake
#
# Provides a function to apply a consistent set of compiler warning flags
# to CMake targets across all supported compilers and platforms.
#
# Usage:
#   include(cmake/Warnings.cmake)
#   logviewer_target_warnings(application_core)
#   logviewer_target_warnings(LogViewer)
#
# The function uses PRIVATE visibility so warning flags don't propagate to
# downstream consumers of library targets.

function(logviewer_target_warnings target)
    if(NOT TARGET ${target})
        message(WARNING "[warnings] Target '${target}' does not exist — skipping")
        return()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4             # High warning level (equivalent to -Wall -Wextra)
            /WX             # Treat warnings as errors
            /permissive-    # Enforce standards conformance
            /w14242         # Narrowing conversion: 'identifier' from 'type1' to 'type1'
            /w14254         # Larger bitfield operator conversion
            /w14263         # Member function does not override base class virtual
            /w14265         # Class has virtual functions but destructor is not virtual
            /w14287         # Unsigned/negative constant mismatch
            /we4289         # Loop control variable used outside for-loop scope
            /w14296         # Expression is always false/true
            /w14311         # Pointer truncation from 'type1' to 'type2'
            /w14545         # Expression before comma evaluates to function missing argument list
            /w14546         # Function call before comma missing argument list
            /w14547         # Operator before comma has no effect
            /w14549         # Operator before comma has no effect (alternative)
            /w14555         # Expression has no effect
            /w14619         # pragma warning: unknown warning number
            /w14640         # Enable warning on thread un-safe static member initialization
            /w14826         # Conversion from 'type1' to 'type2' is sign-extended
            /w14905         # Wide string literal cast to LPSTR
            /w14906         # String literal cast to LPWSTR
            /w14928         # Illegal copy-initialization
            # Suppress warnings from generated Qt code
            /wd4127         # Conditional expression is constant (Qt macros)
            /wd4251         # DLL interface warning (Qt classes)
        )
    else()
        # GCC and Clang share most flags
        target_compile_options(${target} PRIVATE
            -Wall               # Standard warnings
            -Wextra             # Extra warnings
            -Wpedantic          # Strict standard compliance
            -Wshadow            # Variable shadowing
            -Wconversion        # Implicit type conversions (may truncate value)
            -Wsign-conversion   # Signed/unsigned mismatch
            -Wcast-align        # Pointer cast may increase required alignment
            -Wunused            # Unused variables, parameters, functions
            -Woverloaded-virtual # Overloading rather than overriding a virtual function
            -Wnull-dereference  # Potential null pointer dereference
            -Wdouble-promotion  # float implicitly promoted to double
            -Wformat=2          # printf/scanf format string vulnerabilities
            -Wimplicit-fallthrough  # Switch case fall-through without annotation

            # Suppress noisy warnings from Qt-generated code and common patterns
            -Wno-missing-field-initializers  # Qt aggregate-initializes many structs
        )

        # Clang-specific extras
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE
                -Wno-unknown-warning-option  # Gracefully ignore GCC-only flags
            )
        endif()

        # GCC-specific extras
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
            target_compile_options(${target} PRIVATE
                -Wlogical-op          # Suspicious logical operator use
                -Wduplicated-cond     # Identical conditions in if-else chain
                -Wduplicated-branches # Identical code in if-else branches
                -Wmisleading-indentation  # Indentation doesn't match block structure
            )
        endif()
    endif()

    message(STATUS "[warnings] Applied to target: ${target}")
endfunction()
