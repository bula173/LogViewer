#ifndef GUI_ITEMVIRTUALLISTCONTROL_HPP
#define GUI_ITEMVIRTUALLISTCONTROL_HPP

#include "db/EventsContainer.hpp"
#include "mvc/IView.hpp"
#include <wx/listctrl.h>

namespace gui
{
class ItemVirtualListControl : public wxListCtrl, public mvc::IView
{
  public:
    ItemVirtualListControl(db::EventsContainer& events, wxWindow* parent,
        const wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize);

    void RefreshAfterUpdate();
    virtual void OnDataUpdated() override;
    virtual void OnCurrentIndexUpdated(const int index) override;

  private:
    const wxString getColumnName(const int column) const;

  private:
    db::EventsContainer& m_events;

    // Event handlers
    void OnCopyValue(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnColumnResized(wxSizeEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);
    
    // Copy operations
    void CopyValueToClipboard();
    void CopyKeyToClipboard();
    void CopyBothToClipboard();
    
    // Text wrapping
    wxString WrapText(const wxString& text, int width);
    
    // Current data
    std::vector<std::pair<std::string, std::string>> m_currentItems;
};

} // namespace gui

#endif // GUI_ITEMVIRTUALLISTCONTROL_HPP
