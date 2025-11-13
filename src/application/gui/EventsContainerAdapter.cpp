#include "config/Config.hpp"


#include "EventsContainerAdapter.hpp"
#include "gui/EventsVirtualListControl.hpp" // for color helpers, if needed
#include "util/WxWidgetsUtils.hpp"

namespace gui
{
EventsContainerAdapter::EventsContainerAdapter(db::EventsContainer& container)
    : wxDataViewVirtualListModel(0)
    , m_container(container)
    , m_rowCount(0)
{
    SyncWithContainer();
}

unsigned int EventsContainerAdapter::GetColumnCount() const
{
    return static_cast<unsigned int>(config::GetConfig().columns.size());
}

wxString EventsContainerAdapter::GetColumnType(unsigned int WXUNUSED(col)) const
{
    // All columns as string for simplicity
    return "string";
}

void EventsContainerAdapter::GetValueByRow(
    wxVariant& variant, unsigned int row, unsigned int col) const
{
    if (row >= m_rowCount)
    {
        spdlog::warn("GetValueByRow: Row index {} out of bounds (max: {})", row,
            m_rowCount - 1);
        return;
    }

    size_t actualRow = row;
    if (!m_filteredIndices.empty())
    {
        if (row >= m_filteredIndices.size())
            return;
        actualRow = m_filteredIndices[row];
    }

    // Find the Nth visible column from config (where N = col)
    const auto& colConfig = config::GetConfig().columns;
    unsigned int visibleCount = 0;
    size_t configColIdx = 0;

    for (size_t i = 0; i < colConfig.size(); i++)
    {
        if (colConfig[i].isVisible)
        {
            if (visibleCount == col)
            {
                configColIdx = i;
                break;
            }
            visibleCount++;
        }
    }

    // If we couldn't find enough visible columns
    if (visibleCount != col)
    {
        spdlog::warn("GetValueByRow: Not enough visible columns ({}) for "
                     "column index {}",
            visibleCount, col);
        variant = wxVariant("");
        return;
    }


    const auto& columnName = colConfig[configColIdx].name;
    const auto& event = m_container.GetEvent(wx_utils::to_model_index(actualRow));

    if (columnName == "id")
    {
        variant = wxString::FromUTF8(std::to_string(event.getId()).c_str());
    }
    else
    {
        variant = wxString::FromUTF8(event.findByKey(columnName));
    }
}

void EventsContainerAdapter::SyncWithContainer()
{
    m_rowCount = wx_utils::to_wx_uint(m_container.Size());
    Reset(m_rowCount);
}

bool EventsContainerAdapter::SetValueByRow(
    const wxVariant& variant, unsigned int row, unsigned int col)
{
    // Read-only: disallow editing
    // ignore unused parameters
    (void)variant;
    (void)row;
    (void)col;
    return false;
}

void EventsContainerAdapter::SetRowCount(unsigned int WXUNUSED(rows))
{
    SyncWithContainer();
}

void EventsContainerAdapter::SetFilteredIndices(
    const std::vector<unsigned long>& indices)
{

    m_filteredIndices = indices;
    m_rowCount = static_cast<unsigned int>(m_filteredIndices.size());
    Reset(m_rowCount);
}

wxColour EventsContainerAdapter::GetBgColorForColumnValue(
    const std::string& column, const std::string& value) const
{
    const auto& colMap = config::GetConfig().columnColors;
    auto colIt = colMap.find(column);
    if (colIt != colMap.end())
    {
        auto valIt = colIt->second.find(value);
        if (valIt != colIt->second.end())
            return wxColour(valIt->second.bg); // bg
    }
    else
    {
        return wxColour(10, 10, 10); // Default grey if not found
    }
    return wxColour(10, 10, 10); // Default grey if not found
}

wxColour EventsContainerAdapter::GetFontColorForColumnValue(
    const std::string& column, const std::string& value) const
{
    const auto& colMap = config::GetConfig().columnColors;
    auto colIt = colMap.find(column);
    if (colIt != colMap.end())
    {
        auto valIt = colIt->second.find(value);
        if (valIt != colIt->second.end())
            return wxColour(valIt->second.fg); // fg
    }
    else
    {
        return wxColour(255, 255, 255); // Default black if not found
    }
    return wxColour(255, 255, 255); // Default black if not found
}

bool EventsContainerAdapter::GetAttrByRow(
    unsigned int row, unsigned int col, wxDataViewItemAttr& attr) const
{
    // Get the actual event index if filtered
    size_t actualRow = m_filteredIndices.empty() ? row : m_filteredIndices[row];
    const auto& event = m_container.GetEvent(wx_utils::to_model_index(actualRow));

    spdlog::debug(
        "EventsContainerAdapter::GetAttrByRow called for row: {}, col: {}", row,
        col);
    // Example: use "type" column for coloring
    std::string typeValue = event.findByKey("type");

    // Use your config-driven color helpers
    wxColour bg = GetBgColorForColumnValue("type", typeValue);
    wxColour fg = GetFontColorForColumnValue("type", typeValue);

    bool colored = false;
    if (bg.IsOk())
    {
        attr.SetBackgroundColour(bg);
        colored = true;
    }
    if (fg.IsOk())
    {
        attr.SetColour(fg);
        colored = true;
    }
    return colored;
}

bool EventsContainerAdapter::HasFilteredIndices() const
{
    return !m_filteredIndices.empty();
}

size_t EventsContainerAdapter::GetFilteredIndex(unsigned int filteredRow) const
{
    if (m_filteredIndices.empty() || filteredRow >= m_filteredIndices.size())
    {
        return filteredRow; // Fallback to direct index
    }
    return m_filteredIndices[filteredRow];
}

void EventsContainerAdapter::UpdateColors()
{
    spdlog::debug("EventsContainerAdapter::UpdateColors called");
    RefreshRowAttributes();
}

void EventsContainerAdapter::RefreshRowAttributes()
{
    spdlog::debug("EventsContainerAdapter::RefreshRowAttributes called");

    // Try multiple approaches for maximum compatibility
    Cleared();
    Reset(m_rowCount);

    // Also notify value changes for all visible rows
    for (unsigned int i = 0; i < m_rowCount; ++i)
    {
        ItemChanged(GetItem(i));
    }
}
} // namespace gui
