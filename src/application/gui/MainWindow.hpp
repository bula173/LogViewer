#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <wx/wx.h>
#include <wx/splitter.h>

#include "gui/EventsVirtualListControl.hpp"
#include "gui/ItemVirtualListControl.hpp"
#include "db/EventsContainer.hpp"
#include "parser/IDataParser.hpp"
#include "xml/XmlParser.hpp"
#include <atomic>
#include <memory>

namespace gui
{
	enum
	{
		ID_Hello = 1,
		ID_ViewLeftPanel = 2,
		ID_ViewRightPanel = 3

	};

	class MainWindow : public wxFrame, public parser::IDataParserObserver
	{
	public:
		MainWindow(const wxString &title, const wxPoint &pos, const wxSize &size);

	public: // IDataParserObserver
		void ProgressUpdated() override;
		void NewEventFound(db::LogEvent &&event) override;

	private:
		void OnHello(wxCommandEvent &event);
		void OnExit(wxCommandEvent &event);
		void OnClose(wxCloseEvent &event);
		void OnAbout(wxCommandEvent &event);
		void OnSize(wxSizeEvent &event);
		void OnOpen(wxCommandEvent &event);
		void OnHideSearchResult(wxCommandEvent &event);
		void OnHideLeftPanel(wxCommandEvent &event);
		void OnHideRightPanel(wxCommandEvent &event);

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
		std::shared_ptr<parser::IDataParser> m_parser{nullptr};
	};

} // namespace gui

#endif // MAINWINDOW_HPP
