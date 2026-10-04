#pragma once

#include "EventsTableModel.hpp"

#include <QDialog>
#include <QSet>
#include <QString>

#include <optional>
#include <vector>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

namespace ui::qt
{

/// Excel-style value filter for one table column: a search box, a
/// "Select all" check box and one check box per distinct value.
///
/// Emits Applied(checked) on OK (even with every value checked: only the
/// receiver knows whether the old filter restricts values the list could not
/// show), or Cleared() when "Clear filter" is pressed. When the value list was
/// truncated, OK emits AppliedExcluding(unchecked) instead: the values that
/// could not be listed must stay visible.
class ColumnFilterPopup : public QDialog
{
    Q_OBJECT

  public:
    /// @param allowed  currently allowed values; std::nullopt = no filter yet
    ///                 (every value starts checked).
    ColumnFilterPopup(const QString& columnTitle,
        const ColumnDistinctValues& distinct,
        const std::optional<QSet<QString>>& allowed,
        QWidget* parent = nullptr);

    /// Raw values of every checked entry (including ones hidden by the search).
    QSet<QString> CheckedValues() const;
    /// Raw values of every listed entry that is not checked.
    QSet<QString> UncheckedValues() const;

  signals:
    void Applied(const QSet<QString>& allowed);
    void AppliedExcluding(const QSet<QString>& excluded);
    void Cleared();

  private:
    void OnSearchChanged(const QString& text);
    void OnSelectAllToggled();
    void OnItemChanged(QListWidgetItem* item);
    void UpdateSelectAllState();
    void Accept();

    QLineEdit*   m_search    {nullptr};
    QCheckBox*   m_selectAll {nullptr};
    QListWidget* m_list      {nullptr};
    bool         m_updating  {false};
    bool         m_truncated {false};
};

} // namespace ui::qt
