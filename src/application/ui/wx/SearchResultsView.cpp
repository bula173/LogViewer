#include "ui/wx/SearchResultsView.hpp"

#include "mvc/IControler.hpp"
#include "util/WxWidgetsUtils.hpp"

#include <unordered_set>

namespace ui::wx
{

SearchResultsView::SearchResultsView(wxWindow* parent, wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style)
    : wxDataViewListCtrl(parent, id, pos, size, style)
{
    Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
        &SearchResultsView::OnItemActivated, this);
}

void SearchResultsView::SetObserver(ui::ISearchResultsViewObserver* observer)
{
    m_observer = observer;
}

void SearchResultsView::BeginUpdate(const std::vector<std::string>& columns)
{
    if (!m_updatesFrozen)
    {
        Freeze();
        m_updatesFrozen = true;
    }

    DeleteAllItems();
    ClearColumns();
    m_dynamicColumns = BuildColumns(columns);
}

void SearchResultsView::AppendResult(const mvc::SearchResultRow& row)
{
    wxVector<wxVariant> values;
    values.push_back(wxVariant(std::to_string(row.eventId)));
    values.push_back(wxVariant(wxString::FromUTF8(row.matchedKey.c_str())));
    values.push_back(wxVariant(wxString::FromUTF8(row.matchedText.c_str())));

    for (const auto& colValue : row.columnValues)
    {
        values.push_back(wxVariant(wxString::FromUTF8(colValue.c_str())));
    }

    AppendItem(values);
}

void SearchResultsView::EndUpdate()
{
    if (m_updatesFrozen)
    {
        Thaw();
        m_updatesFrozen = false;
    }
}

void SearchResultsView::Clear()
{
    DeleteAllItems();
}

void SearchResultsView::OnItemActivated(wxDataViewEvent& event)
{
    if (!m_observer)
        return;

    const int row = ItemToRow(event.GetItem());
    if (row == wxNOT_FOUND)
        return;

    long eventId = -1;
    if (!TryExtractEventId(row, eventId))
        return;

    m_observer->OnSearchResultActivated(eventId);
}

size_t SearchResultsView::BuildColumns(
    const std::vector<std::string>& columns)
{
    size_t dynamic = 0;

    AppendTextColumn("Event ID");
    AppendTextColumn("Matched Key");
    AppendTextColumn("Match");

    std::unordered_set<std::string> added;
    added.insert("event id");
    added.insert("matched key");
    added.insert("match");

    for (auto column : columns)
    {
        if (column.empty())
            continue;

        for (auto& ch : column)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        if (!added.insert(column).second)
            continue;

        AppendTextColumn(wxString::FromUTF8(column.c_str()));
        ++dynamic;
    }

    return dynamic;
}

bool SearchResultsView::TryExtractEventId(int row, long& eventId) const
{
    if (GetColumnCount() == 0)
        return false;

    wxVariant value;
    const_cast<SearchResultsView*>(this)->GetValue(
        value, wx_utils::int_to_uint(row), 0);
    return value.GetString().ToLong(&eventId);
}

} // namespace ui::wx
