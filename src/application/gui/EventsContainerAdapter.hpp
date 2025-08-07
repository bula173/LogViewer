#pragma once

#include "config/Config.hpp"
#include "db/EventsContainer.hpp"
#include <string>
#include <vector>
#include <wx/dataview.h>

namespace gui
{
class EventsContainerAdapter : public wxDataViewVirtualListModel
{
  public:
    explicit EventsContainerAdapter(db::EventsContainer& container);
    virtual unsigned int GetColumnCount() const override;
    virtual wxString GetColumnType(unsigned int col) const override;
    virtual void GetValueByRow(
        wxVariant& variant, unsigned int row, unsigned int col) const override;
    bool SetValueByRow(
        const wxVariant& variant, unsigned int row, unsigned int col) override;
    void SyncWithContainer();
    void SetRowCount(unsigned int rows);

    /**
     * @brief Set the indices of events to be shown (filtered view).
     * @param indices Vector of indices in the original container to show.
     */
    void SetFilteredIndices(const std::vector<unsigned long>& indices);
    bool GetAttrByRow(unsigned int row, unsigned int col,
        wxDataViewItemAttr& attr) const override;

    wxColour GetFontColorForColumnValue(
        const std::string& column, const std::string& value) const;
    wxColour GetBgColorForColumnValue(
        const std::string& column, const std::string& value) const;

    /**
     * @brief Check if filtered indices are active.
     * @return True if filtering is active
     */
    bool HasFilteredIndices() const;

    /**
     * @brief Get the actual event index from filtered row index.
     * @param filteredRow Row index in the filtered view
     * @return Actual event index in the container
     */
    size_t GetFilteredIndex(unsigned int filteredRow) const;

  private:
    db::EventsContainer& m_container;
    unsigned int m_rowCount;
    std::vector<unsigned long>
        m_filteredIndices; ///< Maps filtered row to container index
};
}