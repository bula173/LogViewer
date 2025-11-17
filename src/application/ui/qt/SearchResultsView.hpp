#pragma once

#include "ui/IUiPanels.hpp"

#include <QTreeWidget>
#include <vector>

class QTreeWidgetItem;

namespace ui::qt
{

class SearchResultsView : public QTreeWidget, public ui::ISearchResultsView
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
    void HandleActivation(QTreeWidgetItem* item, int column);

  private:
    ui::ISearchResultsViewObserver* m_observer {nullptr};
    std::vector<std::string> m_columns;
    static constexpr int kEventIdColumn = 0;
    static constexpr int kMatchedKeyColumn = 1;
    static constexpr int kMatchTextColumn = 2;
};

} // namespace ui::qt
