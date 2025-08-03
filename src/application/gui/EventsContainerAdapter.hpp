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
    void SetFilteredIndices(const std::vector<size_t>& indices);

  private:
    db::EventsContainer& m_container;
    unsigned int m_rowCount;
    std::vector<size_t>
        m_filteredIndices; ///< Maps filtered row to container index
};
}