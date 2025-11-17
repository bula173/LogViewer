/**
 * @file ThirdPartyWarnings.hpp
 * @brief Macros to suppress warnings from third-party libraries
 * 
 * Use THIRD_PARTY_INCLUDES_BEGIN before including third-party headers
 * and THIRD_PARTY_INCLUDES_END after them to suppress compiler warnings
 * that we cannot fix.
 */

#pragma once

// Clang/GCC
#if defined(__clang__) || defined(__GNUC__)
    #define THIRD_PARTY_INCLUDES_BEGIN \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"") \
        _Pragma("GCC diagnostic ignored \"-Wconversion\"") \
        _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"") \
        _Pragma("GCC diagnostic ignored \"-Wformat\"") \
        _Pragma("GCC diagnostic ignored \"-Wcharacter-conversion\"")
    
    #define THIRD_PARTY_INCLUDES_END \
        _Pragma("GCC diagnostic pop")

// MSVC
#elif defined(_MSC_VER)
    #define THIRD_PARTY_INCLUDES_BEGIN \
        __pragma(warning(push)) \
        __pragma(warning(disable: 4100)) /* unreferenced formal parameter */ \
        __pragma(warning(disable: 4127)) /* conditional expression is constant */ \
        __pragma(warning(disable: 4244)) /* conversion, possible loss of data */ \
        __pragma(warning(disable: 4267)) /* conversion from size_t */ \
        __pragma(warning(disable: 4996)) /* deprecated functions */
    
    #define THIRD_PARTY_INCLUDES_END \
        __pragma(warning(pop))

// Other compilers
#else
    #define THIRD_PARTY_INCLUDES_BEGIN
    #define THIRD_PARTY_INCLUDES_END
#endif
