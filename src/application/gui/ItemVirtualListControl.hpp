#ifndef GUI_ITEMVIRTUALLISTCONTROL_HPP
#define GUI_ITEMVIRTUALLISTCONTROL_HPP

#include "db/EventsContainer.hpp"
#include "mvc/IView.hpp"
#include <wx/dataview.h>

namespace gui
{
class ItemVirtualListControl : public wxDataViewListCtrl, public mvc::IView
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

    void OnCopyValue(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnItemHover(wxDataViewEvent& event);
    void OnColumnResized(wxSizeEvent& event);
};

} // namespace gui

#endif // GUI_ITEMVIRTUALLISTCONTROL_HPP
