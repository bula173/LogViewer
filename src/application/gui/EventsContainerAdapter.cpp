#include "config/Config.hpp"


#include "EventsContainerAdapter.hpp"

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
    const std::vector<size_t>& indices)
{

    m_filteredIndices = indices;
    m_rowCount = static_cast<unsigned int>(m_filteredIndices.size());
    Reset(m_rowCount);
}
} // namespace gui
