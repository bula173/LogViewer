/**
 * @file Error.hpp
 * @brief Error handling utilities.
 */

#ifndef ERROR_HPP
#define ERROR_HPP

#include <spdlog/spdlog.h>
#include <string>
#include <wx/msgdlg.h>

namespace error
{

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

/** Error class for handling application-specific errors.
 */
class Error : public std::runtime_error
{
  public:
    explicit Error(const std::string& message, bool showMsgBox = true)
        : std::runtime_error(message)
    {
        if (showMsgBox)
            ShowError(message);
        spdlog::error("Application Error: {}", message);
    }

    explicit Error(const wxString& message, bool showMsgBox = true)
        : std::runtime_error(message.ToStdString())
    {
        if (showMsgBox)
            ShowError(message);
        spdlog::error("Application Error: {}", message.ToStdString());
    }

    explicit Error(const std::wstring& message, bool showMsgBox = true)
        : std::runtime_error(wxString(message).ToStdString())
    {
        if (showMsgBox)
            ShowError(message);
        spdlog::error("Application Error: {}", wxString(message).ToStdString());
    }

    explicit Error(const char* message, bool showMsgBox = true)
        : Error(std::string(message), showMsgBox)
    {
    }
};

} // namespace error

#endif // ERROR_HPP