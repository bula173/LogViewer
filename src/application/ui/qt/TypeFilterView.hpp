#pragma once

#include "ui/IUiPanels.hpp"

#include <QWidget>

#include <functional>

class QListWidget;
class QListWidgetItem;
class QVBoxLayout;

namespace ui::qt
{

/**
 * @brief Qt-backed implementation of the type filter panel.
 */
class TypeFilterView : public QWidget, public ui::ITypeFilterView
{
    Q_OBJECT

  public:
    explicit TypeFilterView(QWidget* parent = nullptr);

    void SetOnFilterChanged(std::function<void()> handler) override;
    void ReplaceTypes(const std::vector<std::string>& types,
        bool checkedByDefault) override;
    void ShowControl(bool show) override;
    void SelectAll() override;
    void DeselectAll() override;
    void InvertSelection() override;
    [[nodiscard]] std::vector<std::string> CheckedTypes() const override;
    [[nodiscard]] bool Empty() const override;

  private:
    void NotifyChanged();
    void SetAll(Qt::CheckState state);

    std::function<void()> m_onFilterChanged;
    QListWidget* m_listWidget {nullptr};
};

} // namespace ui::qt
