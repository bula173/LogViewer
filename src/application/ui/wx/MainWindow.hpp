/**
 * @file MainWindow.hpp
 * @brief Main application window with log viewing and filtering capabilities.
 * @author LogViewer Development Team
 * @date 2025
 */

#pragma once
#include "main/version.h"
#include "db/EventsContainer.hpp"
#include "EventsVirtualListControl.hpp"
#include "ui/wx/SearchResultsView.hpp"
#include "ui/wx/TypeFilterView.hpp"
#include "config/ConfigObserver.hpp"
#include "ui/IEventsView.hpp"
#include "ui/IMainWindowView.hpp"
#include "ui/IUiPanels.hpp"
#include "ui/MainWindowPresenter.hpp"
#include "ui/wx/FiltersPanel.hpp"
#include "mvc/IControler.hpp"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <wx/checklst.h>
#include <wx/config.h>
#include <wx/dataview.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/filehistory.h>
#include <wx/gauge.h>
#include <wx/splitter.h>

/**
 * @namespace ui::wx
 * @brief User interface components and controls.
 */
namespace ui::wx
{

/**
 * @class MainWindow
 * @brief Primary application window providing log viewing and analysis
 * capabilities.
 *
 * MainWindow serves as the central hub for the LogViewer application,
 * integrating data parsing, event display, filtering, and search functionality
 * into a cohesive user interface. It implements the Observer pattern to receive
 * notifications from data parsers and updates the display accordingly.
 *
 * @par Key Features
 * - File opening with drag-and-drop support and recent files menu
 * - Virtual list controls for efficient display of large datasets (millions of
 * events)
 * - Multi-criteria filtering with apply/clear functionality
 * - Full-text search across event data with results panel
 * - Resizable panels with persistent layout preferences
 * - Theme support (light/dark modes)
 * - Progress indication during file parsing
 * - Debug data population for development and testing
 *
 * @par Architecture
 * The window uses a hierarchical splitter-based layout:
 * ```
 * +-----------------------------------------------------------+
 * |                    Menu Bar & Toolbar                    |
 * +-----------------------------------------------------------+
 * | Left Panel    |           Main Content                   |
 * | (Filters)     |  +-----------------------------------+   |
 * |               |  |         Event List                |   |
 * |               |  |       (Virtual List)             |   |
 * |               |  +-----------------------------------+   |
 * |               |  |      Event Details               |   |
 * |               |  |    (Right Panel)                 |   |
 * +-----------------------------------------------------------+
 * |              Search Results (Collapsible)              |
 * +-----------------------------------------------------------+
 * |                    Status Bar                           |
 * +-----------------------------------------------------------+
 * ```
 *
 * @par Thread Safety
 * This class should only be accessed from the main GUI thread. Data parsing
 * operations may run on background threads but UI updates are marshaled to
 * the main thread through the observer pattern.
 *
 * @par Performance Considerations
 * - Uses virtual list controls to handle millions of events efficiently
 * - Implements lazy loading and on-demand data retrieval
 * - Batched event processing to minimize UI update overhead
 * - Progress reporting to maintain UI responsiveness during large file parsing
 */
class MainWindow : public wxFrame,
                   public config::ConfigObserver,
                   public ui::IMainWindowView,
                   public ui::ISearchResultsViewObserver
{
  public:
    /**
     * @brief Constructs the main application window.
     *
     * Initializes all UI components, sets up the layout, creates menus and
     * toolbars, and configures event handlers. The window is created but not
     * shown.
     *
     * @param title Window title to display in the title bar
     * @param pos Initial window position on screen
     * @param size Initial window size
     * @param version Application version information for display in about
     * dialog
     *
     * @note The version parameter is stored by reference, so the Version object
     *       must remain valid for the lifetime of the MainWindow
     */
    MainWindow(const wxString& title, const wxPoint& pos, const wxSize& size,
        const Version::Version& version);

    // ConfigObserver interface implementation
    virtual void OnConfigChanged() override;

  private:
    // Event handler method declarations

    /**
     * @brief Handles window close events.
     *
     * Saves window state, stops any active parsing operations, and
     * performs cleanup before allowing the window to close.
     *
     * @param event The close event (can be vetoed if necessary)
     */
    void OnClose(wxCloseEvent& event);

    /**
     * @brief Handles window size change events.
     *
     * Updates internal layout and saves window dimensions for next session.
     *
     * @param event The size event containing new dimensions
     */
    void OnSize(wxSizeEvent& event);

    /**
     * @brief Handles File->Exit menu selection.
     *
     * Initiates application shutdown by closing the main window.
     *
     * @param event The command event from the menu selection
     */
    void OnExit(wxCommandEvent& event);

    /**
     * @brief Handles Help->About menu selection.
     *
     * Displays the application about dialog with version information,
     * credits, and licensing details.
     *
     * @param event The command event from the menu selection
     */
    void OnAbout(wxCommandEvent& event);

    /**
     * @brief Handles File->Open menu selection.
     *
     * Shows file dialog for selecting log files to parse and display.
     * Supports various file formats based on available parsers.
     *
     * @param event The command event from the menu selection
     */
    void OnOpenFile(wxCommandEvent& event);

    /**
     * @brief Handles recent file menu selections.
     *
     * Opens and parses a file from the recent files history.
     * Files are automatically added to history when opened successfully.
     *
     * @param event The command event containing the file history ID
     */
    void OnRecentFile(wxCommandEvent& event);

    /**
     * @brief Handles search button clicks and search text entry.
     *
     * Performs full-text search across all event data and populates
     * the search results panel with matching events.
     *
     * @param event The command event from search button or text ctrl
     */
    void OnSearch(wxCommandEvent& event);

    /**
     * @brief Handles double-clicks on search results.
     *
     * Navigates to the selected search result in the main event list
     * and highlights the corresponding event.
     *
     * @param event The data view event from the search results list
     */
    void OnSearchResultActivated(long eventId) override;

    /**
     * @brief Handles Apply Filter button clicks.
     *
     * Applies current filter settings to the event list, creating
     * a filtered view of events based on selected criteria.
     *
     * @param event The command event from the apply filter button
     */
    void OnApplyFilter(wxCommandEvent& event);

    /**
     * @brief Handles Clear Filter button clicks.
     *
     * Resets all filter controls to show all events and clears
     * any active filtering from the display.
     *
     * @param event The command event from the clear filter button
     */
    void OnClearFilter(wxCommandEvent& event);

    /**
     * @brief Handles View->Hide Search Results menu selection.
     *
     * Toggles visibility of the bottom search results panel.
     * Panel state is preserved for the session.
     *
     * @param event The command event from the menu selection
     */
    void OnHideSearchResult(wxCommandEvent& event);

    /**
     * @brief Handles View->Hide Left Panel menu selection.
     *
     * Toggles visibility of the left filter panel.
     * Panel state is preserved for the session.
     *
     * @param event The command event from the menu selection
     */
    void OnHideLeftPanel(wxCommandEvent& event);

    /**
     * @brief Handles View->Hide Right Panel menu selection.
     *
     * Toggles visibility of the right event details panel.
     * Panel state is preserved for the session.
     *
     * @param event The command event from the menu selection
     */
    void OnHideRightPanel(wxCommandEvent& event);

    /**
     * @brief Handles Parser->Clear menu selection.
     *
     * Clears all loaded events and resets the application state
     * for loading new data.
     *
     * @param event The command event from the menu selection
     */
    void OnClearParser(wxCommandEvent& event);

    /**
     * @brief Handles Tools->Config Editor menu selection.
     *
     * Opens the application configuration file in the system's
     * default editor for JSON files.
     *
     * @param event The command event from the menu selection
     */
    void OnOpenConfigEditor(wxCommandEvent& event);

    /**
     * @brief Handles Tools->App Log Viewer menu selection.
     *
     * Opens the application's internal log file for debugging
     * and troubleshooting purposes.
     *
     * @param event The command event from the menu selection
     */
    void OnOpenAppLog(wxCommandEvent& event);

    void OnFilterChanged(wxCommandEvent&);
    void HandleTypeFilterChanged();

    /**
     * @brief Handles Theme->Light menu selection.
     *
     * Switches the application to light theme mode and updates
     * all UI components accordingly.
     *
     * @param event The command event from the menu selection
     */
    void OnThemeLight(wxCommandEvent& event);

    /**
     * @brief Handles Theme->Dark menu selection.
     *
     * Switches the application to dark theme mode and updates
     * all UI components accordingly.
     *
     * @param event The command event from the menu selection
     */
    void OnThemeDark(wxCommandEvent& event);
    /**
     * @brief Handles drag-and-drop file events.
     *
     * Processes files dropped onto the main window, parsing the first
     * dropped file and updating the UI accordingly.
     *
     * @param event The drop files event containing the dropped file paths
     */
    void OnDropFiles(wxDropFilesEvent& event);

    /**
     * @brief Applies dark theme recursively to all child windows.
     *
     * Sets background and foreground colors for the specified window
     * and all its children to match the dark theme.
     *
     * @param window The root window to apply the dark theme to
     */
    void ApplyDarkThemeRecursive(wxWindow* window);

    /**
     * @brief Applies light theme recursively to all child windows.
     *
     * Sets background and foreground colors for the specified window
     * and all its children to match the light theme.
     *
     * @param window The root window to apply the light theme to
     */
    void ApplyLightThemeRecursive(wxWindow* window);


#ifndef NDEBUG
    /**
     * @brief Handles Debug->Populate Dummy Data menu selection (debug builds
     * only).
     *
     * Generates sample log events for testing and development purposes.
     * Only available in debug builds to prevent accidental use in production.
     *
     * @param event The command event from the menu selection
     */
    void OnPopulateDummyData(wxCommandEvent& event);
#endif

    // Private utility methods

    /**
     * @brief Initializes the application menu bar.
     *
     * Creates and configures the main menu with File, Edit, View, Parser,
     * Tools, and Help submenus. Includes recent files integration and
     * platform-specific menu conventions.
     */
    void setupMenu();

    /**
     * @brief Initializes the main window layout.
     *
     * Creates and arranges all UI controls including splitter windows,
     * virtual list controls, filter panels, and status bar. Configures
     * the overall window layout with proper sizing and constraints.
     */
    void setupLayout();

    /**
     * @brief Initializes the status bar.
     *
     * Creates a status bar with message area and progress gauge for
     * displaying application status and parsing progress indication.
     */
    void setupStatusBar();

    /**
     * @brief Adds a file path to the recent files history.
     *
     * Updates the recent files menu and saves the new entry to persistent
     * storage for future sessions.
     *
     * @param path The file path to add to the history
     */
    void AddToRecentFiles(const wxString& path);

    /** @brief Centralized file loading hook shared by all entry points. */
    void ProcessFileSelection(const std::filesystem::path& filePath);

    /**
     * @brief Loads a recent file from the history.
     *
     * @param event The command event from the menu selection
     */
    void LoadRecentFile(wxCommandEvent& event);

    /** @brief @return true when background processing is active. */
    bool IsBusy() const;
    /**
     * @brief Reloads the application configuration from disk.
     *
     * Re-reads the configuration file and updates all UI components
     * based on the new settings. Useful for applying changes made in
     * the config editor without restarting the application.
     */
    void OnReloadConfig(wxCommandEvent&);

    // ui::IMainWindowView implementation
    std::string ReadSearchQuery() const override;
    std::string CurrentStatusText() const override;
    void UpdateStatusText(const std::string& text) override;
    void SetSearchControlsEnabled(bool enabled) override;
    void ToggleProgressVisibility(bool visible) override;
    void ConfigureProgressRange(int range) override;
    void UpdateProgressValue(int value) override;
    void ProcessPendingEvents() override;
    void RefreshLayout() override;

    // UI Control member variables

    ui::wx::EventsVirtualListControl* m_eventsListCtrl {
        nullptr}; ///< Main event list display (center panel)
    wxWindow* m_itemDetailsWindow {
        nullptr}; ///< Selected event details view (right panel)
    ui::IEventsListView* m_eventsListView {
        nullptr}; ///< Toolkit-agnostic handle for events list view
    ui::IItemDetailsView* m_itemDetailsView {
        nullptr}; ///< Toolkit-agnostic handle for details panel
    ui::wx::SearchResultsView* m_searchResultsList {
        nullptr}; ///< wx implementation of search results view
    ui::ISearchResultsView* m_searchResultsView {
        nullptr}; ///< Toolkit-agnostic search results view
    wxPanel* m_leftPanel {nullptr}; ///< Left filter and control panel
    wxPanel* m_searchResultPanel {
        nullptr}; ///< Bottom search results panel (collapsible)
    wxSplitterWindow* m_bottom_spliter {
        nullptr}; ///< Bottom splitter for search results visibility
    wxSplitterWindow* m_left_spliter {
        nullptr}; ///< Left splitter for filter panel visibility
    wxSplitterWindow* m_rigth_spliter {
        nullptr}; ///< Right splitter for details panel and main list
    wxGauge* m_progressGauge {nullptr}; ///< Progress bar embedded in status bar
    wxTextCtrl* m_searchBox {
        nullptr}; ///< Search input field with enter key support
    wxButton* m_searchButton {nullptr}; ///< Search execute button
    ui::wx::TypeFilterView* m_typeFilterPanel {nullptr}; ///< wx type filter view
    ui::ITypeFilterView* m_typeFilterView {nullptr}; ///< Toolkit-agnostic type filter
    wxButton* m_applyFilterButton {
        nullptr}; ///< Apply current filter settings button
    wxButton* m_clearFilterButton {
        nullptr}; ///< Clear all filters and show all events button
    ui::wx::FiltersPanel* m_filtersPanel = nullptr; ///< Filters panel instance

    // Data and state member variables

    const Version::Version&
        m_version; ///< Application version information reference
    wxFileHistory
        m_fileHistory; ///< Recent files manager (persistent across sessions)
    db::EventsContainer
        m_events; ///< Main events collection (all loaded events)
    std::atomic<bool> m_processing {
        false}; ///< Non-parser processing flag (debug/dummy data)
    std::atomic<bool> m_closerequest {
        false}; ///< Window close requested flag (thread-safe)
    std::unique_ptr<mvc::IController> m_controller;
    std::unique_ptr<ui::MainWindowPresenter> m_presenter;
    // Menu and control identifiers

    /**
     * @brief Custom menu and control IDs for event handling.
     *
     * These IDs are used to identify specific menu items and controls
     * in event handlers. Values start above wxWidgets reserved range.
     */
    enum
    {
        ID_PopulateDummyData =
            1001,          ///< Debug menu item for generating test data
        ID_ViewLeftPanel,  ///< View menu item for toggling left panel
        ID_ViewRightPanel, ///< View menu item for toggling right panel
        ID_ParserClear,    ///< Parser menu item for clearing data
        ID_ConfigEditor,   ///< Tools menu item for config editing
        ID_AppLogViewer,   ///< Tools menu item for viewing application logs
        ID_ParserReloadConfig = wxID_HIGHEST + 200
    };

  public:
    // Called by the FiltersPanel to apply filters
    void ApplyFilters();
};

} // namespace ui::wx
