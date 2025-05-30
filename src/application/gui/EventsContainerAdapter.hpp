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

  private:
    db::EventsContainer& m_container;
    unsigned int m_rowCount;
};
}