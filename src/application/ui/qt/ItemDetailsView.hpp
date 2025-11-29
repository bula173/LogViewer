#pragma once

#include "mvc/IView.hpp"
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

class ItemDetailsView : public QWidget,
                        public ui::IItemDetailsView,
                        public mvc::IView
{
    Q_OBJECT

  public:
    explicit ItemDetailsView(
        db::EventsContainer& events, QWidget* parent = nullptr);

    void RefreshView() override;
    void ShowControl(bool show) override;

    void OnDataUpdated() override;
    void OnCurrentIndexUpdated(const int index) override;

  public slots:
    void OnActualRowChanged(int actualRow);

  private:
    void DisplayEvent(int actualRow);

    db::EventsContainer& m_events;
    QTableWidget* m_details {nullptr};
};

} // namespace ui::qt
