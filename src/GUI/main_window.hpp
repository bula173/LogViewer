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
		void OnSize(wxSizeEvent &event);
		wxDECLARE_EVENT_TABLE();

		void setupMenu();
		void setupLayout();

		void populateData();

	private:
		gui::EventsVirtualListControl *m_eventsListCtrl{nullptr};
		db::EventsContainer m_events;
		wxGauge *m_progressGauge{nullptr};
		const long m_eventsNum{10000};
	};

} // namespace gui

#endif // MAINWINDOW_HPP
