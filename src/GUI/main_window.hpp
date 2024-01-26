#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <wx/wx.h>

#include "events_virtual_list_control.hpp"
#include "DB/events_container.hpp"

namespace gui
{
	enum
	{
		ID_Hello = 1
	};

	class MainWindow : public wxFrame
	{
	public:
		MainWindow(const wxString &title, const wxPoint &pos, const wxSize &size);

	private:
		void OnHello(wxCommandEvent &event);
		void OnExit(wxCommandEvent &event);
		void OnAbout(wxCommandEvent &event);
		wxDECLARE_EVENT_TABLE();

	private:
		gui::EventsVirtualListControl *m_eventsContainer;
		db::EventsContainer m_events;
	};

} // namespace gui

#endif // MAINWINDOW_HPP
