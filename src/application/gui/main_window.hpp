#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <wx/wx.h>
#include <wx/splitter.h>

#include "gui/events_virtual_list_control.hpp"
#include "gui/item_list_view.hpp"
#include "db/events_container.hpp"

#include <atomic>

namespace gui
{
	enum
	{
		ID_Hello = 1,
		ID_ViewLeftPanel = 2,
		ID_ViewRightPanel = 3

	};

	class MainWindow : public wxFrame
	{
	public:
		MainWindow(const wxString &title, const wxPoint &pos, const wxSize &size);

	private:
		void OnHello(wxCommandEvent &event);
		void OnExit(wxCommandEvent &event);
		void OnClose(wxCloseEvent &event);
		void OnAbout(wxCommandEvent &event);
		void OnSize(wxSizeEvent &event);
		void OnHideSearchResult(wxCommandEvent &event);
		void OnHideLeftPanel(wxCommandEvent &event);
		void OnHideRightPanel(wxCommandEvent &event);

		wxDECLARE_EVENT_TABLE();

		void setupMenu();
		void setupLayout();
		void setupStatusBar();
		void populateData();

	private:
		gui::EventsVirtualListControl *m_eventsListCtrl{nullptr};
		gui::ItemVirtualListControl *m_itemView{nullptr};
		wxPanel *m_leftPanel{nullptr};
		wxPanel *m_searchResultPanel{nullptr};
		wxSplitterWindow *m_bottom_spliter{nullptr};
		wxSplitterWindow *m_left_spliter{nullptr};
		wxSplitterWindow *m_rigth_spliter{nullptr};
		db::EventsContainer m_events;
		wxGauge *m_progressGauge{nullptr};
		const long m_eventsNum{100000};

		std::atomic<bool> m_closerequest{false};
		bool m_processing{false};
	};

} // namespace gui

#endif // MAINWINDOW_HPP
