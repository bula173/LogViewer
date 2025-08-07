#include "config/Config.hpp"


#include "EventsContainerAdapter.hpp"
#include "gui/EventsVirtualListControl.hpp" // for color helpers, if needed

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

wxString EventsContainerAdapter::GetColumnType(unsigned int col) const
{
    // All columns as string for simplicity
    return "string";
}

void EventsContainerAdapter::GetValueByRow(
    wxVariant& variant, unsigned int row, unsigned int col) const
{
    if (!(row < m_rowCount))
        return;

    size_t actualRow = row;
    if (!m_filteredIndices.empty())
    {
        if (row >= m_filteredIndices.size())
            return;
        actualRow = m_filteredIndices[row];
    }

    const auto& event = m_container.GetEvent(actualRow);
    auto& colConfig = config::GetConfig().columns;
    if (col >= colConfig.size() || !colConfig[col].visible)
    {
        variant = wxVariant();
        return;
    }
    if (col == 0)
    {
        variant = wxString::FromUTF8(std::to_string(event.getId()));
        return;
    }
    variant = wxString::FromUTF8(event.findByKey(colConfig[col].name));
}

void EventsContainerAdapter::SyncWithContainer()
{
    m_rowCount = m_container.Size();
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

void EventsContainerAdapter::SetRowCount(unsigned int rows)
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
    const auto& event = m_container.GetEvent(actualRow);

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
} // namespace gui
