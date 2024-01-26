#ifndef GUI_EVENTSVIRTUALLISTCONTROL_HPP
#define GUI_EVENTSVIRTUALLISTCONTROL_HPP

#include <wx/wx.h>
#include <wx/listctrl.h>

#include "DB/events_container.hpp"

namespace gui
{
	class EventsVirtualListControl : public wxListCtrl
	{
	public:
		EventsVirtualListControl(wxWindow *parent, const wxWindowID id, const wxPoint &pos, const wxSize &size, const db::EventsContainer &events);

		virtual wxString OnGetItemText(long index, long column) const wxOVERRIDE;
		void RefreshAfterUpdate();

	private:
		const db::EventsContainer &m_events;
	};

} // namespace gui

#endif // GUI_EVENTSVIRTUALLISTCONTROL_HPP