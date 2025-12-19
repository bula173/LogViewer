/**
 * @file Error.hpp
 * @brief Error handling utilities.
 */

#pragma once

#include "UiServices.hpp"
#include "Logger.hpp"

#include <atomic>
#include <stdexcept>
#include <string>
#include <string_view>

namespace error
{

/**
 * @brief Error code enumeration for categorizing errors
 */
enum class ErrorCode
{
    Unknown,           ///< Unknown or unspecified error
    InvalidArgument,   ///< Invalid argument provided
    RuntimeError,      ///< Runtime error occurred
    NotImplemented,    ///< Feature not yet implemented
    FileNotFound,      ///< File not found
    ParseError,        ///< Parsing error
    IOError            ///< Input/output error
};

// Global toggle for showing dialogs (default: true)
inline std::atomic<bool>& DialogsEnabledFlag()
{
    static std::atomic<bool> flag {true};
    return flag;
}
inline void SetShowDialogs(bool enabled)
{
    DialogsEnabledFlag().store(enabled);
}
inline bool GetShowDialogs()
{
    return DialogsEnabledFlag().load();
}
inline bool CanShowDialogs()
{
    auto presenter = ui::UiServices::Instance().GetErrorPresenter();
    return GetShowDialogs() && presenter && presenter->CanShowDialogs();
}

inline void ShowError(std::string_view message)
{
    auto presenter = ui::UiServices::Instance().GetErrorPresenter();
    if (presenter && CanShowDialogs()) {
        presenter->ShowError(message);
    }
}

class Error : public std::runtime_error
{
  private:
    ErrorCode m_code;

  public:
    explicit Error(ErrorCode code, const char* message, bool showMsgBox = true)
        : Error(code, std::string(message), showMsgBox)
    {
    }

    explicit Error(ErrorCode code, const std::string& message, bool showMsgBox = true)
        : std::runtime_error(message)
        , m_code(code)
    {
        if (showMsgBox && CanShowDialogs())
            ShowError(message);
        util::Logger::Error("Application Error [{}]: {}",
            static_cast<int>(code), message);
    }

    // Legacy constructors without ErrorCode (defaults to Unknown)
    explicit Error(const char* message, bool showMsgBox = true)
        : Error(ErrorCode::Unknown, std::string(message), showMsgBox)
    {
    }

    explicit Error(const std::string& message, bool showMsgBox = true)
        : Error(ErrorCode::Unknown, message, showMsgBox)
    {
    }

    ErrorCode code() const { return m_code; }
};

} // namespace error