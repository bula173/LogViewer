#ifndef MAINWINDOWS_HPP
#define MAINWINDOWS_HPP

#include <wx/wx.h>

enum
{
	ID_Hello = 1
};

class MainWindows : public wxFrame
{
public:
	MainWindows(const wxString &title, const wxPoint &pos, const wxSize &size);

private:
	void OnHello(wxCommandEvent &event);
	void OnExit(wxCommandEvent &event);
	void OnAbout(wxCommandEvent &event);
	wxDECLARE_EVENT_TABLE();
};

#endif // MAINWINDOWS_HPP
