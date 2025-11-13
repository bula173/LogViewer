/**
 * @file Error.hpp
 * @brief Error handling utilities.
 */

#pragma once

#include <atomic>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

// Add the missing wx headers for wxTheApp and wxIsMainThread
#include <wx/app.h> // wxTheApp
#include <wx/msgdlg.h>
#include <wx/string.h>
#include <wx/thread.h> // wxIsMainThread

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

inline void ShowError(const wxString& message)
{
    wxMessageBox(message, "Error", wxOK | wxICON_ERROR);
}
inline void ShowError(const std::string& message)
{
    ShowError(wxString::FromUTF8(message.c_str()));
}
inline void ShowError(const std::wstring& message)
{
    ShowError(wxString(message));
}

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
    // Only show if enabled, app exists, and we're on the main thread
    return GetShowDialogs() && wxTheApp != nullptr && wxIsMainThread();
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
        spdlog::error("Application Error [{}]: {}", static_cast<int>(code), message);
    }

    explicit Error(ErrorCode code, const wxString& message, bool showMsgBox = true)
        : std::runtime_error(message.ToStdString())
        , m_code(code)
    {
        if (showMsgBox && CanShowDialogs())
            ShowError(message);
        spdlog::error("Application Error [{}]: {}", static_cast<int>(code), message.ToStdString());
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

    explicit Error(const wxString& message, bool showMsgBox = true)
        : Error(ErrorCode::Unknown, message, showMsgBox)
    {
    }

    ErrorCode code() const { return m_code; }
};

} // namespace error