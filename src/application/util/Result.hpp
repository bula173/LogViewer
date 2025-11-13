/**
 * @file Result.hpp
 * @brief Modern error handling with Result<T, E> type
 * @author LogViewer Development Team
 * @date 2025
 *
 * Provides a Result<T, E> type for explicit error handling without exceptions.
 * This is particularly useful in performance-critical paths where exception
 * overhead is unacceptable.
 *
 * @par Example Usage:
 * @code
 * Result<Config, ConfigError> LoadConfig(const std::string& path) {
 *     if (!std::filesystem::exists(path)) {
 *         return Err(ConfigError("File not found: " + path));
 *     }
 *     
 *     Config config = // ... load config ...
 *     return Ok(std::move(config));
 * }
 *
 * // Usage:
 * auto result = LoadConfig("config.json");
 * if (result.isOk()) {
 *     Config config = result.unwrap();
 *     // Use config
 * } else {
 *     spdlog::error("Failed: {}", result.error().what());
 * }
 * @endcode
 */

#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace util {

/**
 * @class Result
 * @brief A type that represents either success (Ok) or failure (Err)
 *
 * Result<T, E> is a modern C++ implementation of Rust's Result type.
 * It forces explicit error handling at compile time and avoids the
 * overhead of C++ exceptions in hot paths.
 *
 * @tparam T The type of the success value
 * @tparam E The type of the error value
 *
 * @par Thread Safety:
 * Result is not thread-safe. Each thread should have its own instance.
 *
 * @par Performance:
 * Zero overhead compared to manual error code checking when optimized.
 * Uses std::variant internally for type-safe storage.
 */
template<typename T, typename E>
class Result {
public:
    /**
     * @brief Constructs a successful result
     * @param value The success value (moved)
     * @return Result containing the success value
     */
    static Result Ok(T value)
    {
        Result result;
        result.m_data = std::move(value);
        return result;
    }

    /**
     * @brief Constructs an error result
     * @param error The error value (moved)
     * @return Result containing the error
     */
    static Result Err(E error)
    {
        Result result;
        result.m_data = std::move(error);
        return result;
    }

    /**
     * @brief Checks if the result contains a success value
     * @return true if Ok, false if Err
     */
    [[nodiscard]] bool isOk() const noexcept
    {
        return std::holds_alternative<T>(m_data);
    }

    /**
     * @brief Checks if the result contains an error
     * @return true if Err, false if Ok
     */
    [[nodiscard]] bool isErr() const noexcept
    {
        return std::holds_alternative<E>(m_data);
    }

    /**
     * @brief Unwraps the success value
     * @return The success value (moved)
     * @throws std::logic_error if called on an Err result
     */
    [[nodiscard]] T unwrap()
    {
        if (isErr()) {
            throw std::logic_error("Called unwrap() on an Err result");
        }
        return std::move(std::get<T>(m_data));
    }

    /**
     * @brief Unwraps the success value or returns a default
     * @param defaultValue Value to return if result is Err
     * @return The success value or defaultValue
     */
    [[nodiscard]] T unwrapOr(T defaultValue) const
    {
        if (isOk()) {
            return std::get<T>(m_data);
        }
        return std::move(defaultValue);
    }

    /**
     * @brief Unwraps the success value or computes a default
     * @tparam F Callable type that returns T
     * @param fn Function to call if result is Err
     * @return The success value or result of fn()
     */
    template<typename F>
    [[nodiscard]] T unwrapOrElse(F&& fn)
    {
        if (isOk()) {
            return std::move(std::get<T>(m_data));
        }
        return fn();
    }

    /**
     * @brief Gets the error value
     * @return The error value
     * @throws std::logic_error if called on an Ok result
     */
    [[nodiscard]] const E& error() const
    {
        if (isOk()) {
            throw std::logic_error("Called error() on an Ok result");
        }
        return std::get<E>(m_data);
    }

    /**
     * @brief Maps the success value to a new type
     * @tparam F Callable type: T -> U
     * @param fn Transformation function
     * @return Result<U, E> with transformed value or original error
     */
    template<typename F>
    [[nodiscard]] auto map(F&& fn) -> Result<decltype(fn(std::declval<T>())), E>
    {
        using U = decltype(fn(std::declval<T>()));
        if (isOk()) {
            return Result<U, E>::Ok(fn(std::move(std::get<T>(m_data))));
        }
        return Result<U, E>::Err(std::get<E>(m_data));
    }

    /**
     * @brief Maps the error value to a new type
     * @tparam F Callable type: E -> F
     * @param fn Transformation function
     * @return Result<T, F> with original value or transformed error
     */
    template<typename F>
    [[nodiscard]] auto mapErr(F&& fn) -> Result<T, decltype(fn(std::declval<E>()))>
    {
        using F2 = decltype(fn(std::declval<E>()));
        if (isErr()) {
            return Result<T, F2>::Err(fn(std::get<E>(m_data)));
        }
        return Result<T, F2>::Ok(std::move(std::get<T>(m_data)));
    }

    /**
     * @brief Chains operations that return Results
     * @tparam F Callable type: T -> Result<U, E>
     * @param fn Transformation function
     * @return Result<U, E> from fn or original error
     */
    template<typename F>
    [[nodiscard]] auto andThen(F&& fn) -> decltype(fn(std::declval<T>()))
    {
        if (isOk()) {
            return fn(std::move(std::get<T>(m_data)));
        }
        return decltype(fn(std::declval<T>()))::Err(std::get<E>(m_data));
    }

    /**
     * @brief Provides an alternative result if this is an error
     * @tparam F Callable type: E -> Result<T, E>
     * @param fn Function to call on error
     * @return Original Ok value or result of fn(error)
     */
    template<typename F>
    [[nodiscard]] Result orElse(F&& fn)
    {
        if (isErr()) {
            return fn(std::get<E>(m_data));
        }
        return Result::Ok(std::move(std::get<T>(m_data)));
    }

    /**
     * @brief Converts Result to std::optional, discarding error
     * @return Optional containing value if Ok, empty otherwise
     */
    [[nodiscard]] std::optional<T> ok() const
    {
        if (isOk()) {
            return std::get<T>(m_data);
        }
        return std::nullopt;
    }

    /**
     * @brief Converts Result to std::optional of error
     * @return Optional containing error if Err, empty otherwise
     */
    [[nodiscard]] std::optional<E> err() const
    {
        if (isErr()) {
            return std::get<E>(m_data);
        }
        return std::nullopt;
    }

    /**
     * @brief Boolean conversion operator
     * @return true if Ok, false if Err
     */
    explicit operator bool() const noexcept { return isOk(); }

private:
    Result() = default;
    std::variant<T, E> m_data; ///< Storage for either value or error
};

/**
 * @brief Helper function to create an Ok result
 * @tparam T Value type
 * @tparam E Error type
 * @param value The success value
 * @return Result<T, E> containing the value
 */
template<typename T, typename E>
Result<T, E> Ok(T value)
{
    return Result<T, E>::Ok(std::move(value));
}

/**
 * @brief Helper function to create an Err result
 * @tparam T Value type
 * @tparam E Error type
 * @param error The error value
 * @return Result<T, E> containing the error
 */
template<typename T, typename E>
Result<T, E> Err(E error)
{
    return Result<T, E>::Err(std::move(error));
}

} // namespace util
