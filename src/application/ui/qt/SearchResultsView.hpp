#pragma once

#include "ui/IUiPanels.hpp"

#include <QListWidget>
#include <vector>

class QListWidgetItem;

namespace ui::qt
{

class SearchResultsView : public QListWidget, public ui::ISearchResultsView
{
    Q_OBJECT

  public:
    explicit SearchResultsView(QWidget* parent = nullptr);

    void SetObserver(ui::ISearchResultsViewObserver* observer) override;
    void BeginUpdate(const std::vector<std::string>& columns) override;
    void AppendResult(const mvc::SearchResultRow& row) override;
    void EndUpdate() override;
    void Clear() override;

    /** @brief Helper that mimics a user double-clicking a row. */
    void SimulateActivation(long eventId);

  private slots:
    void HandleActivation(QListWidgetItem* item);

  private:
    ui::ISearchResultsViewObserver* m_observer {nullptr};
    std::vector<std::string> m_columns;
};

} // namespace ui::qt
