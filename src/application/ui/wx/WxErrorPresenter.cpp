#include "ui/wx/WxErrorPresenter.hpp"

#include <wx/app.h>
#include <wx/msgdlg.h>
#include <wx/string.h>
#include <wx/thread.h>

namespace ui::wx {

bool WxErrorPresenter::CanShowDialogs() const noexcept
{
    return wxTheApp != nullptr && wxIsMainThread();
}

void WxErrorPresenter::ShowError(std::string_view message)
{
    const std::string ownedMessage(message);
    wxMessageBox(wxString::FromUTF8(ownedMessage.c_str()), "Error",
        wxOK | wxICON_ERROR);
}

} // namespace ui::wx
