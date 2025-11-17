#ifndef GUI_ITEMVIRTUALLISTCONTROL_HPP
#define GUI_ITEMVIRTUALLISTCONTROL_HPP

#include "mvc/IModel.hpp"
#include "mvc/IView.hpp"
#include "ui/IUiPanels.hpp"
#include "ui/wx/CustomDataModel.hpp"
#include "util/Logger.hpp"
#include <wx/dataview.h>
#include <memory>

namespace ui::wx
{
/**
 * @brief Data view showing key/value pairs with wrapped text.
 *
 * Uses a custom data model and renderer to provide dynamic row heights
 * so long log values remain readable without tooltips.
 */
class ItemVirtualListControl : public wxDataViewCtrl,
                               public mvc::IView,
                               public ui::IItemDetailsView
{
  public:
    ItemVirtualListControl(mvc::IModel& events, wxWindow* parent,
        const wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize);
    ~ItemVirtualListControl() override;

    /** @brief Repopulate the control after the backing event changes. */
    void RefreshAfterUpdate();
    /** @brief mvc::IView hook notifying that data mutated. */
    void OnDataUpdated() override;
    /** @brief mvc::IView hook notifying that selection changed. */
    void OnCurrentIndexUpdated(const int index) override;

    /** @brief Implements toolkit-agnostic refresh hook. */
    void RefreshView() override;

    /** @brief Show or hide the control to honor interface contract. */
    void ShowControl(bool show) override;

  private:
    const wxString getColumnName(const int column) const;

  private:
    mvc::IModel& m_events;
    std::unique_ptr<CustomDataModel> m_model;
    int m_lastWrapWidth = -1;

    // Event handlers
    void OnKeyDown(wxKeyEvent& event);
    void OnColumnResized(wxSizeEvent& event);
    void OnContextMenu(wxContextMenuEvent& event);
    void OnItemSelected(wxDataViewEvent& event);
    void OnItemActivated(wxDataViewEvent& event);
    void EnsureFirstRowSelected();
    
    // Copy operations
    /** @brief Copy selected value into system clipboard. */
    void CopyValueToClipboard();
    /** @brief Copy selected key into system clipboard. */
    void CopyKeyToClipboard();
    /** @brief Copy selected key/value pair into system clipboard. */
    void CopyBothToClipboard();
    
    // Setup methods
    /** @brief Helper that wires up the key and value columns. */
    void SetupColumns();
    /** @brief Recomputes the wrapping renderer width after column resize. */
    void UpdateWrappingWidth();
};

} // namespace ui::wx

#endif // GUI_ITEMVIRTUALLISTCONTROL_HPP
