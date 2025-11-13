#include "gui/EventsVirtualListControl.hpp"
#include "config/Config.hpp"
#include "util/WxWidgetsUtils.hpp"
#include "util/Logger.hpp"
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
    util::Logger::Debug(
        "EventsVirtualListControl::EventsVirtualListControl constructed");

    m_events.RegisterOndDataUpdated(this);

    m_model = new gui::EventsContainerAdapter(m_events);
    m_model->SetRowCount(static_cast<unsigned int>(m_events.Size()));
    this->AssociateModel(m_model);

    // Use the common method instead of duplicating code
    AddColumnsFromConfig();

    this->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
        [this](wxDataViewEvent& evt)
        {
            unsigned int filteredRowIndex = m_model->GetRow(evt.GetItem());
            util::Logger::Debug("EventsVirtualListControl::wxEVT_DATAVIEW_SELECTION_"
                          "CHANGED filtered row index: {}",
                filteredRowIndex);

            // Convert filtered row index to actual event index
            size_t actualEventIndex;
            if (m_model->HasFilteredIndices())
            {
                actualEventIndex = m_model->GetFilteredIndex(filteredRowIndex);
                util::Logger::Debug(
                    "Converted to actual event index: {}", actualEventIndex);
            }
            else
            {
                actualEventIndex = filteredRowIndex;
            }

            this->m_events.SetCurrentItem(actualEventIndex);
        });
}

void EventsVirtualListControl::OnDataUpdated()
{
    util::Logger::Debug("EventsVirtualListControl::OnDataUpdated called");
    m_model->SetRowCount(static_cast<unsigned int>(m_events.Size()));
    m_model->Reset(static_cast<unsigned int>(m_events.Size()));
}

void EventsVirtualListControl::RefreshAfterUpdate()
{
    util::Logger::Debug("EventsVirtualListControl::RefreshAfterUpdate called");
    m_model->SetRowCount(static_cast<unsigned int>(m_events.Size()));
    m_model->Reset(static_cast<unsigned int>(m_events.Size()));
}

void EventsVirtualListControl::OnCurrentIndexUpdated(const int index)
{
    util::Logger::Debug(
        "EventsVirtualListControl::OnCurrentIndexUpdated called with index: {}",
        index);

    // The index passed here is the actual container/model index.
    // If a filter is active we must convert it back to the filtered row
    // index used by the view model before calling GetItem().
    unsigned int rowIndex = wx_utils::int_to_uint(index);
    if (m_model && m_model->HasFilteredIndices())
    {
        // Search for the filtered row which maps to the actual index.
        bool found = false;
        const unsigned int count = m_model->GetCount();
        for (unsigned int i = 0; i < count; ++i)
        {
            if (m_model->GetFilteredIndex(i) == static_cast<size_t>(index))
            {
                rowIndex = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            util::Logger::Debug(
                "EventsVirtualListControl::OnCurrentIndexUpdated - actual index {} not present in filtered indices",
                index);
            // If not found, clear selection and return.
            this->UnselectAll();
            return;
        }
    }

    wxDataViewItem item = m_model->GetItem(rowIndex);
    this->Select(item);
    this->EnsureVisible(item);
    this->SetCurrentItem(item);
    this->SetFocus();
}

void EventsVirtualListControl::SetFilteredEvents(
    const std::vector<unsigned long>& filteredIndices)
{
    util::Logger::Debug(
        "EventsVirtualListControl::SetFilteredEvents called, count: {}",
        filteredIndices.size());
    m_model->SetFilteredIndices(filteredIndices);
    m_model->SetRowCount(static_cast<unsigned int>(filteredIndices.size()));
    m_model->Reset(static_cast<unsigned int>(filteredIndices.size()));
}

void EventsVirtualListControl::UpdateColors()
{
    util::Logger::Debug("EventsVirtualListControl::UpdateColors called");

    if (m_model)
    {
        m_model->UpdateColors();
    }

    // Force complete visual refresh
    this->Refresh();
    this->Update();

    // Ensure immediate repaint
    this->wxWindow::Refresh();
    this->wxWindow::Update();
}

void EventsVirtualListControl::RefreshColumns()
{
    util::Logger::Debug("EventsVirtualListControl::RefreshColumns called");

    // Get config columns that should be visible
    const auto& colConfig = config::GetConfig().columns;
    std::vector<const config::ColumnConfig*> visibleConfigColumns;

    for (const auto& col : colConfig)
    {
        if (col.isVisible)
        {
            visibleConfigColumns.push_back(&col);
        }
    }

    // Freeze UI updates
    Freeze();

    // Step 1: Update existing columns where possible
    int existingCount = wx_utils::uint_to_int(GetColumnCount());
    int configCount = static_cast<int>(visibleConfigColumns.size());
    int commonCount = std::min(existingCount, configCount);

    // Update common columns (reuse existing ones)
    for (int i = 0; i < commonCount; i++)
    {
        wxDataViewColumn* col = GetColumn(wx_utils::int_to_uint(i));
        const config::ColumnConfig* configCol = visibleConfigColumns[static_cast<size_t>(i)];

        // Update column properties
        if (col)
        {
            col->SetTitle(configCol->name);
            col->SetWidth(configCol->width);
        }
    }

    // Step 2: Remove extra columns if we have too many
    if (existingCount > configCount)
    {
        for (int i = existingCount - 1; i >= configCount; i--)
        {
            wxDataViewColumn* col = GetColumn(wx_utils::int_to_uint(i));
            if (col)
            {
                util::Logger::Debug("Removing extra column at index {}", i);
                DeleteColumn(col);
            }
        }
    }

    // Step 3: Add missing columns if we have too few
    if (configCount > existingCount)
    {
        for (int i = existingCount; i < configCount; i++)
        {
            const config::ColumnConfig* configCol = visibleConfigColumns[static_cast<size_t>(i)];
            util::Logger::Debug("Adding missing column: {}", configCol->name);

            auto renderer =
                new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
            auto column = new wxDataViewColumn(configCol->name, renderer, wx_utils::int_to_uint(i),
                configCol->width, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);

            AppendColumn(column);
        }
    }

    // IMPORTANT: Force column width recalculation to eliminate dead space
    // Either use column width auto-resize...
    for (unsigned int i = 0; i < GetColumnCount(); i++)
    {
        wxDataViewColumn* col = GetColumn(wx_utils::uint_to_int(i));
        if (col)
        {
            col->SetWidth(col->GetWidth()); // Force recalculation
        }
    }

    // Refresh the model data
    if (m_model)
    {
        m_model->Reset(static_cast<unsigned int>(m_model->GetCount()));
    }

    // IMPORTANT: Force complete layout update - more aggressive than just
    // Layout()
    wxSize curSize = GetSize();
    SetSize(curSize.x + 1, curSize.y); // Trigger size change
    SetSize(curSize);                  // Restore original size

    // Resume UI updates
    Thaw();

    // Normal layout call may not be sufficient
    Layout();

    // Ask parent to resize us as well
    if (GetParent() && GetParent()->GetSizer())
    {
        GetParent()->Layout();
    }
}

// Add this private method
void EventsVirtualListControl::AddColumnsFromConfig()
{
    const auto& colConfig = config::GetConfig().columns;

    // Create vector of pointers to visible columns
    std::vector<const config::ColumnConfig*> visibleConfigColumns;
    for (const auto& col : colConfig)
    {
        if (col.isVisible)
        {
            visibleConfigColumns.push_back(&col);
        }
    }

    // Add columns
    unsigned int colIndex = 0;
    for (const auto* col : visibleConfigColumns)
    {
        util::Logger::Debug("Adding column: {}.", col->name);
        try
        {
            auto renderer = new wxDataViewTextRenderer(
                "string", wxDATAVIEW_CELL_INERT, wxDVR_DEFAULT_ALIGNMENT);
            auto column = new wxDataViewColumn(col->name, renderer, colIndex,
                col->width, wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);

            if (!AppendColumn(column))
            {
                util::Logger::Error("Failed to append column: {}", col->name);
                delete column; // Clean up if append fails
            }
            else
            {
                ++colIndex;
            }
        }
        catch (const std::exception& e)
        {
            util::Logger::Error(
                "Exception adding column {}: {}", col->name, e.what());
        }
    }
}

// Add this helper to count expected columns
unsigned int EventsVirtualListControl::CountVisibleConfigColumns() const
{
    const auto& colConfig = config::GetConfig().columns;
    int visible = 0;
    for (const auto& col : colConfig)
    {
        if (col.isVisible)
            visible++;
    }
    return visible;
}

} // namespace gui
