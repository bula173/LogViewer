#include "gui/CustomDataModel.hpp"
#include "util/Logger.hpp"
#include <string>

namespace gui
{

CustomDataModel::CustomDataModel(db::EventsContainer& events)
    : wxDataViewVirtualListModel()
    , m_events(events)
{
    UpdateDataCache();
}

unsigned int CustomDataModel::GetColumnCount() const
{
    return 2; // Key and Value columns
}

wxString CustomDataModel::GetColumnType(unsigned int) const
{
    return wxT("string");
}

void CustomDataModel::GetValueByRow(wxVariant& variant, unsigned int row, unsigned int col) const
{
    if (row >= m_data.size())
    {
        variant = wxVariant();
        return;
    }

    const auto& [key, value] = m_data[row];
    static int accessLogCount = 0;
    if (accessLogCount < 5)
    {
        util::Logger::Debug("CustomDataModel::GetValueByRow row {} col {}", row, col);
        ++accessLogCount;
    }
    
    if (col == 0) // Key column
    {
        variant = wxString::FromUTF8(key);
    }
    else if (col == 1) // Value column
    {
        // Return the full value for multi-line display
        wxString fullValue = wxString::FromUTF8(value);
        static int modelLogCount = 0;
        if (modelLogCount < 5)
        {
            util::Logger::Debug("CustomDataModel value sample: {}",
                wxString(fullValue).Truncate(80).ToStdString());
            ++modelLogCount;
        }
        variant = fullValue;
    }
    else
    {
        variant = wxVariant();
    }
}

bool CustomDataModel::SetValueByRow(const wxVariant&, unsigned int, unsigned int)
{
    // Read-only model
    return false;
}

unsigned int CustomDataModel::GetCount() const
{
    return static_cast<unsigned int>(m_data.size());
}

void CustomDataModel::RefreshData()
{
    UpdateDataCache();
    Reset(GetCount());
}

wxString CustomDataModel::GetOriginalValue(unsigned int row) const
{
    if (row >= m_data.size())
        return wxEmptyString;
    
    return wxString::FromUTF8(m_data[row].second);
}

wxString CustomDataModel::GetOriginalKey(unsigned int row) const
{
    if (row >= m_data.size())
        return wxEmptyString;
    
    return wxString::FromUTF8(m_data[row].first);
}

void CustomDataModel::UpdateDataCache()
{
    m_data.clear();

    const int currentIndex = m_events.GetCurrentItemIndex();
    const auto eventsCount = static_cast<int>(m_events.Size());
    if (currentIndex < 0 || currentIndex >= eventsCount)
    {
        util::Logger::Debug("CustomDataModel::UpdateDataCache skipped - invalid index {} (size = {})",
            currentIndex, eventsCount);
        return;
    }

    const auto& event = m_events.GetEvent(currentIndex);
    const auto& items = event.getEventItems();

    for (const auto& [key, value] : items)
    {
        m_data.emplace_back(key, value);
    }

    util::Logger::Debug("CustomDataModel updated with {} items", m_data.size());
}

} // namespace gui