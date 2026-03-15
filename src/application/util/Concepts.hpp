/**
 * @file Concepts.hpp
 * @brief C++20 concept definitions for LogViewer.
 *
 * This header provides reusable C++20 concepts that enable cleaner,
 * more readable template constraints throughout the codebase.
 *
 * @author LogViewer Development Team
 * @date 2025
 */

#ifndef UTIL_CONCEPTS_HPP
#define UTIL_CONCEPTS_HPP

#include <concepts>
#include <utility>
#include <nlohmann/json.hpp>

/**
 * @namespace util
 * @brief Utility components and helper types for the LogViewer application.
 */
namespace util
{

/**
 * @concept Constructible
 * @brief Matches types that can be constructed from the given arguments.
 *
 * A more readable alternative to std::enable_if_t with std::is_constructible_v.
 *
 * @tparam T The type to construct
 * @tparam Args The argument types
 *
 * @par Example
 * @code
 * template <typename T, typename... Args>
 * requires Constructible<T, Args...>
 * void create(Args&&... args) {
 *     T instance(std::forward<Args>(args)...);
 * }
 * @endcode
 */
template <typename T, typename... Args>
concept Constructible = requires(Args&&... args) {
    T(std::forward<Args>(args)...);
};

/**
 * @concept EventItemsLike
 * @brief Matches types that behave like LogEvent's EventItems collection.
 *
 * EventItems is a std::vector<std::pair<std::string, std::string>>.
 * This concept identifies types that support the same operations.
 *
 * @tparam T The type to check
 *
 * @par Example
 * @code
 * template <typename T>
 * void processItems(const T& items)
 * requires EventItemsLike<T>
 * {
 *     for (const auto& [key, value] : items) {
 *         // Process key-value pair
 *     }
 * }
 * @endcode
 */
template <typename T>
concept EventItemsLike = requires(T t) {
    { t.size() } -> std::convertible_to<size_t>;
    { t.begin() };
    { t.end() };
    requires std::is_same_v<typename T::value_type, std::pair<std::string, std::string>>;
};

/**
 * @concept Comparable
 * @brief Matches types that can be compared for equality.
 *
 * @tparam T The type to check
 */
template <typename T>
concept Comparable = requires(const T& a, const T& b) {
    { a == b } -> std::convertible_to<bool>;
    { a != b } -> std::convertible_to<bool>;
};

/**
 * @concept Hashable
 * @brief Matches types that can be used as hash map keys.
 *
 * @tparam T The type to check
 */
template <typename T>
concept Hashable = requires(const T& a) {
    { std::hash<T>{}(a) } -> std::convertible_to<size_t>;
};

/**
 * @concept Orderable
 * @brief Matches types that support ordering operations.
 *
 * @tparam T The type to check
 */
template <typename T>
concept Orderable = Comparable<T> && requires(const T& a, const T& b) {
    { a < b } -> std::convertible_to<bool>;
    { a <= b } -> std::convertible_to<bool>;
    { a > b } -> std::convertible_to<bool>;
    { a >= b } -> std::convertible_to<bool>;
};

/**
 * @concept HasToString
 * @brief Matches types that provide a to_string() method.
 *
 * @tparam T The type to check
 */
template <typename T>
concept HasToString = requires(const T& t) {
    { t.ToString() } -> std::convertible_to<std::string>;
};

} // namespace util

#endif // UTIL_CONCEPTS_HPP

