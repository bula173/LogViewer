#include "gui/EventsVirtualListControl.hpp"
#include "config/Config.hpp"
#include <spdlog/spdlog.h>
#include <string>
#include <wx/colour.h>

namespace gui
{


EventsVirtualListControl::EventsVirtualListControl(db::EventsContainer& events,
    wxWindow* parent, const wxWindowID id, const wxPoint& pos,
    const wxSize& size)
    : wxDataViewCtrl(
          parent, id, pos, size, wxDV_ROW_LINES | wxDV_VERT_RULES | wxDV_SINGLE)
    , m_events(events)
{
    spdlog::debug(
        "EventsVirtualListControl::EventsVirtualListControl constructed");

    auto& colConfig = config::GetConfig().columns;

    m_events.RegisterOndDataUpdated(this);

    m_model = new gui::EventsContainerAdapter(m_events);
    m_model->SetRowCount(static_cast<unsigned int>(m_events.Size()));
    this->AssociateModel(m_model);


    int colIndex = 0;
    for (const auto& col : colConfig)
    {
        if (col.visible)
        {
            spdlog::debug("Adding column: {}", col.name);
            auto renderer = new wxDataViewTextRenderer(
                "string", wxDATAVIEW_CELL_INERT, wxDVR_DEFAULT_ALIGNMENT);
            auto column = new wxDataViewColumn(col.name, renderer, colIndex,
                col.width, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
            this->AppendColumn(column);
            ++colIndex;
        }
    }

    this->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
        [this](wxDataViewEvent& evt)
        {
            unsigned int item = m_model->GetRow(evt.GetItem());
            spdlog::debug("EventsVirtualListControl::wxEVT_DATAVIEW_SELECTION_"
                          "CHANGED index: {}",
                item);
            this->m_events.SetCurrentItem(item);
        });
}

void EventsVirtualListControl::OnDataUpdated()
{
    spdlog::debug("EventsVirtualListControl::OnDataUpdated called");
    m_model->SetRowCount(static_cast<unsigned int>(m_events.Size()));
    m_model->Reset(static_cast<unsigned int>(m_events.Size()));
}

void EventsVirtualListControl::RefreshAfterUpdate()
{
    spdlog::debug("EventsVirtualListControl::RefreshAfterUpdate called");
    m_model->SetRowCount(static_cast<unsigned int>(m_events.Size()));
    m_model->Reset(static_cast<unsigned int>(m_events.Size()));
}

void EventsVirtualListControl::OnCurrentIndexUpdated(const int index)
{
    spdlog::debug(
        "EventsVirtualListControl::OnCurrentIndexUpdated called with index: {}",
        index);
    wxDataViewItem item = m_model->GetItem(index);
    this->Select(item);
    this->EnsureVisible(item);
    this->SetCurrentItem(item);
    this->SetFocus();
}

void EventsVirtualListControl::SetFilteredEvents(
    const std::vector<size_t>& filteredIndices)
{
    spdlog::debug(
        "EventsVirtualListControl::SetFilteredEvents called, count: {}",
        filteredIndices.size());
    m_model->SetFilteredIndices(filteredIndices);
    m_model->SetRowCount(static_cast<unsigned int>(filteredIndices.size()));
    m_model->Reset(static_cast<unsigned int>(filteredIndices.size()));
}
} // namespace gui
