#pragma once

#include "filters/FilterManager.hpp"
#include "filters/Filter.hpp"

#include <QWidget>

class QTableWidget;
class QTableWidgetItem;
class QPushButton;

namespace ui::qt
{

class FiltersPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit FiltersPanel(QWidget* parent = nullptr);

    void RefreshFilters();

  signals:
    void RequestApplyFilters();

  private slots:
    void HandleAdd();
    void HandleEdit();
    void HandleRemove();
    void HandleApply();
    void HandleClear();
    void HandleSaveAs();
    void HandleLoad();
    void HandleItemChanged(QTableWidgetItem* item);
    void HandleSelectionChanged();

  private:
    void BuildUi();
    void ToggleControls();
    void HandleFilterResult(const filters::FilterResult& result,
      const QString& context, bool showDialog = false);

    QString FilterTargetText(const filters::Filter& filter) const;
    QString SelectedFilterName() const;

    QTableWidget* m_table {nullptr};
    QPushButton* m_addButton {nullptr};
    QPushButton* m_editButton {nullptr};
    QPushButton* m_removeButton {nullptr};
    QPushButton* m_applyButton {nullptr};
    QPushButton* m_clearButton {nullptr};
    QPushButton* m_saveAsButton {nullptr};
    QPushButton* m_loadButton {nullptr};
};

} // namespace ui::qt
