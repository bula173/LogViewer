/**
 * @file TypeFilterView.hpp
 * @brief Qt widget implementing the @c ITypeFilterView interface.
 * @author LogViewer Development Team
 * @date 2026
 */
#pragma once

#include "IUiPanels.hpp"

#include <QWidget>

#include <functional>

class QListWidget;
class QListWidgetItem;
class QVBoxLayout;

namespace ui::qt
{

/**
 * @brief Qt-backed implementation of the log-event type filter panel.
 *
 * Displays a scrollable, checkable list of event types (e.g. ERROR, INFO, WARN).
 * The list is populated by @c ReplaceTypes() whenever a new log file is loaded.
 * Checking/unchecking triggers the @c m_onFilterChanged callback, which drives
 * the async filter pipeline in MainWindowPresenter.
 *
 * @par Relation to ITypeFilterView
 * All public methods except @c SetCheckedTypes() are part of the
 * @c ITypeFilterView toolkit-agnostic interface.  @c SetCheckedTypes() is a
 * Qt-specific extension used by @c FilterProfilesPanel to restore type
 * selections from a saved profile.
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

    /**
     * @brief Restores a type selection from a saved filter profile.
     *
     * Checks only the types whose string representation is present in
     * @p types, unchecking everything else.  Triggers no @c FilterChanged
     * callback — the caller is responsible for re-applying the filter
     * (e.g. via @c MainWindow::OnApplyFilterClicked()) after this call.
     *
     * @param types Type strings to check.  An empty vector is treated as
     *              "check everything" (equivalent to @c SelectAll()).
     */
    void SetCheckedTypes(const std::vector<std::string>& types);

  private:
    void NotifyChanged();
    void SetAll(Qt::CheckState state);

    std::function<void()> m_onFilterChanged;
    QListWidget* m_listWidget {nullptr};
};

} // namespace ui::qt
