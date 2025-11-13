/**
 * @file WxWidgetsUtils.hpp
 * @brief Type-safe conversion utilities for wxWidgets API interactions
 * 
 * This header provides helper functions to safely convert between standard C++
 * types (size_t, int) and wxWidgets-specific types (long, unsigned int) while
 * avoiding compiler warnings and ensuring safety.
 */

#pragma once

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace wx_utils {

/**
 * @brief Safely convert size_t to long for wxWidgets APIs
 * @param value The size_t value to convert
 * @return long The converted value, clamped to LONG_MAX if necessary
 * @note Clamps to LONG_MAX to prevent overflow
 */
inline long to_wx_long(size_t value) noexcept
{
    return static_cast<long>(std::min(value, static_cast<size_t>(LONG_MAX)));
}

/**
 * @brief Safely convert size_t to int for wxWidgets APIs
 * @param value The size_t value to convert
 * @return int The converted value, clamped to INT_MAX if necessary
 * @note Clamps to INT_MAX to prevent overflow
 */
inline int to_wx_int(size_t value) noexcept
{
    return static_cast<int>(std::min(value, static_cast<size_t>(INT_MAX)));
}

/**
 * @brief Safely convert size_t to unsigned int for wxWidgets APIs
 * @param value The size_t value to convert
 * @return unsigned int The converted value, clamped to UINT_MAX if necessary
 * @note Clamps to UINT_MAX to prevent overflow
 */
inline unsigned int to_wx_uint(size_t value) noexcept
{
    return static_cast<unsigned int>(std::min(value, static_cast<size_t>(UINT_MAX)));
}

/**
 * @brief Safely convert long to size_t
 * @param value The long value to convert
 * @return size_t The converted value, 0 if negative
 * @note Returns 0 for negative values to maintain unsigned semantics
 */
inline size_t from_wx_long(long value) noexcept
{
    return value < 0 ? 0 : static_cast<size_t>(value);
}

/**
 * @brief Safely convert int to size_t
 * @param value The int value to convert
 * @return size_t The converted value, 0 if negative
 * @note Returns 0 for negative values to maintain unsigned semantics
 */
inline size_t from_wx_int(int value) noexcept
{
    return value < 0 ? 0 : static_cast<size_t>(value);
}

/**
 * @brief Safely convert unsigned int to size_t
 * @param value The unsigned int value to convert
 * @return size_t The converted value
 */
inline size_t from_wx_uint(unsigned int value) noexcept
{
    return static_cast<size_t>(value);
}

/**
 * @brief Safely convert int to unsigned int for wxWidgets APIs
 * @param value The int value to convert
 * @return unsigned int The converted value, 0 if negative
 * @note Returns 0 for negative values
 */
inline unsigned int int_to_uint(int value) noexcept
{
    return value < 0 ? 0U : static_cast<unsigned int>(value);
}

/**
 * @brief Safely convert unsigned int to int for wxWidgets APIs
 * @param value The unsigned int value to convert
 * @return int The converted value, clamped to INT_MAX if necessary
 * @note Clamps to INT_MAX to prevent overflow
 */
inline int uint_to_int(unsigned int value) noexcept
{
    return static_cast<int>(std::min(value, static_cast<unsigned int>(INT_MAX)));
}

/**
 * @brief Safely convert size_t to int for IModel interface
 * @param value The size_t value to convert
 * @return int The converted value, clamped to INT_MAX
 * @note This is specifically for the IModel::GetEvent(int) interface
 */
inline int to_model_index(size_t value) noexcept
{
    return to_wx_int(value);
}

/**
 * @brief Validate that a size_t value fits in an int
 * @param value The size_t value to validate
 * @throws std::overflow_error if value > INT_MAX
 */
inline void validate_int_range(size_t value)
{
    if (value > static_cast<size_t>(INT_MAX)) {
        throw std::overflow_error("Value exceeds INT_MAX");
    }
}

/**
 * @brief Validate that a size_t value fits in a long
 * @param value The size_t value to validate
 * @throws std::overflow_error if value > LONG_MAX
 */
inline void validate_long_range(size_t value)
{
    if (value > static_cast<size_t>(LONG_MAX)) {
        throw std::overflow_error("Value exceeds LONG_MAX");
    }
}

} // namespace wx_utils
