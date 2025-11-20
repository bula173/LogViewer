#pragma once

#include "ui/IUiPanels.hpp"

#include <wx/dataview.h>

namespace ui::wx
{

class SearchResultsView : public wxDataViewListCtrl,
                           public ui::ISearchResultsView
{
  public:
    SearchResultsView(wxWindow* parent, wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDV_ROW_LINES | wxDV_VERT_RULES | wxDV_SINGLE);

    void SetObserver(ui::ISearchResultsViewObserver* observer) override;
    void BeginUpdate(const std::vector<std::string>& columns) override;
    void AppendResult(const mvc::SearchResultRow& row) override;
    void EndUpdate() override;
    void Clear() override;

  private:
    void OnItemActivated(wxDataViewEvent& event);
    size_t BuildColumns(const std::vector<std::string>& columns);
    bool TryExtractEventId(int row, long& eventId) const;

    ui::ISearchResultsViewObserver* m_observer {nullptr};
    size_t m_dynamicColumns {0};
    bool m_updatesFrozen {false};
};

} // namespace ui::wx
