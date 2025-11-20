#pragma once

#include "ui/IUiPanels.hpp"

#include <QWidget>

class QTableWidget;
class QTableWidgetItem;

namespace db
{
class EventsContainer;
}

namespace ui::qt
{

class ItemDetailsView : public QWidget, public ui::IItemDetailsView
{
    Q_OBJECT

  public:
    explicit ItemDetailsView(
        db::EventsContainer& events, QWidget* parent = nullptr);

    void RefreshView() override;
    void ShowControl(bool show) override;

  public slots:
    void OnActualRowChanged(int actualRow);

  private:
    void DisplayEvent(int actualRow);

    db::EventsContainer& m_events;
    QTableWidget* m_details {nullptr};
};

} // namespace ui::qt
