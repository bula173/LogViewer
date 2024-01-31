#ifndef GUI_EVENTSVIRTUALLISTCONTROL_HPP
#define GUI_EVENTSVIRTUALLISTCONTROL_HPP

#include <wx/wx.h>
#include <wx/listctrl.h>
#include "MVC/view.hpp"

#include "DB/events_container.hpp"

namespace gui
{
	class EventsVirtualListControl : public wxListCtrl, public mvc::View
	{
	public:
		EventsVirtualListControl(db::EventsContainer &events, wxWindow *parent, const wxWindowID id = wxID_ANY,
														 const wxPoint &pos = wxDefaultPosition, const wxSize &size = wxDefaultSize);

		virtual wxString OnGetItemText(long index, long column) const wxOVERRIDE;
		void RefreshAfterUpdate();

		// implement View interface
		virtual void OnDataUpdated() override;

	private:
		const wxString getColumnName(const int column) const;

	private:
		db::EventsContainer &m_events;
	};

} // namespace gui

#endif // GUI_EVENTSVIRTUALLISTCONTROL_HPP
