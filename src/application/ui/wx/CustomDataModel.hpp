#ifndef GUI_CUSTOMDATAMODEL_HPP
#define GUI_CUSTOMDATAMODEL_HPP

#include "mvc/IModel.hpp"
#include <wx/dataview.h>
#include <vector>
#include <utility>

namespace ui::wx
{

/**
 * @brief wxDataViewVirtualListModel exposing key/value pairs for a single event.
 */
class CustomDataModel : public wxDataViewVirtualListModel
{
public:
    CustomDataModel(mvc::IModel& events);
    virtual ~CustomDataModel() = default;

    // wxDataViewVirtualListModel interface
    /** @brief Model shows two columns (key and value). */
    virtual unsigned int GetColumnCount() const override;
    virtual wxString GetColumnType(unsigned int col) const override;
    virtual void GetValueByRow(wxVariant& variant, unsigned int row, unsigned int col) const override;
    virtual bool SetValueByRow(const wxVariant& variant, unsigned int row, unsigned int col) override;
    virtual unsigned int GetCount() const override;

    // Custom methods
    /** @brief Refresh cache from current event in EventsContainer. */
    void RefreshData();
    /** @brief Access the stored value (full text) for the given row. */
    wxString GetOriginalValue(unsigned int row) const;
    /** @brief Access the stored key for the given row. */
    wxString GetOriginalKey(unsigned int row) const;

private:
    mvc::IModel& m_events;
    std::vector<std::pair<std::string, std::string>> m_data;
    
    /** @brief Internal helper rebuilding the cached vector. */
    void UpdateDataCache();
};

} // namespace ui::wx

#endif // GUI_CUSTOMDATAMODEL_HPP