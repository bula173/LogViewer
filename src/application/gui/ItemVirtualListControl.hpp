#ifndef GUI_ITEMVIRTUALLISTCONTROL_HPP
#define GUI_ITEMVIRTUALLISTCONTROL_HPP

#include <wx/dataview.h>
#include <wx/wx.h>

#include "db/EventsContainer.hpp"
#include "mvc/IView.hpp"

namespace gui
{
class ItemVirtualListControl : public wxDataViewListCtrl, public mvc::IView
{
  public:
    ItemVirtualListControl(db::EventsContainer& events, wxWindow* parent,
        const wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize);

    void RefreshAfterUpdate();

    // implement IView interface
    virtual void OnDataUpdated() override;
    virtual void OnCurrentIndexUpdated(const int index) override;

  private:
    const wxString getColumnName(const int column) const;

  private:
    db::EventsContainer& m_events;

    void OnContextMenu(wxDataViewEvent& event);
    void OnCopyValue(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);
};

} // namespace gui

#endif // GUI_ITEMVIRTUALLISTCONTROL_HPP
