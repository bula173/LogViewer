#pragma once

#include "dbc/DbcParser.hpp"

#include <QWidget>

#include <set>
#include <string>
#include <vector>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace ui::qt {

/// Read/write tree panel showing the CAN message/signal structure from a
/// loaded DBC file.  Checkboxes let the user select which signals to plot;
/// checking a frame checks or unchecks all its signals.
///
/// Lives in its own dock widget tabbed alongside the main Filters dock.
/// Call SetDatabase() whenever a new DBC file is loaded.
/// Emits SignalSelectionChanged() whenever the checked set changes.
class CanSignalTreePanel : public QWidget
{
    Q_OBJECT

  public:
    explicit CanSignalTreePanel(QWidget* parent = nullptr);

    /// Replace the displayed DBC structure.  Rebuilds the tree immediately,
    /// preserving any signal selections that still exist in the new database.
    void SetDatabase(const parser::dbc::DbcDatabase& db);

    /// Return the full "SIG:xxx" keys of all currently checked signals.
    std::vector<std::string> GetSelectedSignals() const;

  Q_SIGNALS:
    /// Emitted whenever the checked signal set changes.
    void SignalSelectionChanged();

  private:
    void BuildLayout();
    void RebuildTree();

    parser::dbc::DbcDatabase m_db;

    QTreeWidget* m_tree        {nullptr};
    QLabel*      m_placeholder {nullptr};
};

} // namespace ui::qt
