#pragma once

#include <expected>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

namespace util {

/// A Result<T,E> type backed by std::expected<T,E> (C++23).
/// Carries either a success value (Ok) or an error (Err).
template<typename T, typename E>
class Result {
public:
    [[nodiscard]] static Result Ok(T value)
    {
        return Result{std::expected<T,E>{std::in_place, std::move(value)}};
    }

    [[nodiscard]] static Result Err(E error)
    {
        return Result{std::expected<T,E>{std::unexpect, std::move(error)}};
    }

    [[nodiscard]] bool isOk()  const noexcept { return  m_data.has_value(); }
    [[nodiscard]] bool isErr() const noexcept { return !m_data.has_value(); }
    explicit operator bool()   const noexcept { return  m_data.has_value(); }

    [[nodiscard]] T unwrap()
    {
        if (!m_data.has_value())
            throw std::logic_error("Called unwrap() on an Err Result");
        return std::move(*m_data);
    }

    [[nodiscard]] T unwrapOr(T defaultValue) const
    {
        return m_data.value_or(std::move(defaultValue));
    }

    template<typename F>
    [[nodiscard]] T unwrapOrElse(F&& fn)
    {
        return m_data.has_value() ? std::move(*m_data)
                                  : std::invoke(std::forward<F>(fn));
    }

    [[nodiscard]] const E& error() const
    {
        if (m_data.has_value())
            throw std::logic_error("Called error() on an Ok Result");
        return m_data.error();
    }

    template<typename F>
    [[nodiscard]] auto map(F&& fn)
    {
        using U = std::invoke_result_t<F, T>;
        if (m_data.has_value())
            return Result<U,E>::Ok(std::invoke(std::forward<F>(fn), std::move(*m_data)));
        return Result<U,E>::Err(m_data.error());
    }

    template<typename F>
    [[nodiscard]] auto mapErr(F&& fn)
    {
        using E2 = std::invoke_result_t<F, E>;
        if (m_data.has_value())
            return Result<T,E2>::Ok(std::move(*m_data));
        return Result<T,E2>::Err(std::invoke(std::forward<F>(fn), m_data.error()));
    }

    template<typename F>
    [[nodiscard]] auto andThen(F&& fn) -> std::invoke_result_t<F, T>
    {
        using Ret = std::invoke_result_t<F, T>;
        if (m_data.has_value())
            return std::invoke(std::forward<F>(fn), std::move(*m_data));
        return Ret::Err(m_data.error());
    }

    template<typename F>
    [[nodiscard]] Result orElse(F&& fn)
    {
        if (!m_data.has_value())
            return std::invoke(std::forward<F>(fn), m_data.error());
        return Result::Ok(std::move(*m_data));
    }

    [[nodiscard]] std::optional<T> ok() const
    {
        if (m_data.has_value()) return *m_data;
        return std::nullopt;
    }

    [[nodiscard]] std::optional<E> err() const
    {
        if (!m_data.has_value()) return m_data.error();
        return std::nullopt;
    }

private:
    explicit Result(std::expected<T,E> data) : m_data(std::move(data)) {}
    std::expected<T,E> m_data;
};

// ---------------------------------------------------------------------------
// Void specialisation — uses std::expected<void,E> directly (C++23).
// Callers construct with Result<void,E>::Ok({}) or just ::Ok().
// ---------------------------------------------------------------------------
template<typename E>
class Result<void, E> {
public:
    [[nodiscard]] static Result Ok(std::monostate = {})
    {
        return Result{std::expected<void,E>{}};
    }

    [[nodiscard]] static Result Err(E error)
    {
        return Result{std::expected<void,E>{std::unexpect, std::move(error)}};
    }

    [[nodiscard]] bool isOk()  const noexcept { return  m_data.has_value(); }
    [[nodiscard]] bool isErr() const noexcept { return !m_data.has_value(); }
    explicit operator bool()   const noexcept { return  m_data.has_value(); }

    [[nodiscard]] const E& error() const
    {
        if (m_data.has_value())
            throw std::logic_error("Called error() on an Ok Result<void,E>");
        return m_data.error();
    }

private:
    explicit Result(std::expected<void,E> data) : m_data(std::move(data)) {}
    std::expected<void,E> m_data;
};

} // namespace util
