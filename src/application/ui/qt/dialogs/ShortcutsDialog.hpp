#pragma once

#include <QDialog>

#include <string>
#include <vector>

class QTableWidget;
class QPushButton;

namespace ui::qt {

/**
 * @brief Dialog showing every registered keyboard shortcut, grouped by
 *        category (File, Tools, Analysis, View, Help).
 *
 * @par Data source
 * Rows are populated from @c ShortcutManager::GetAll(), so the list always
 * reflects the application's actual current bindings — not a hand-maintained
 * copy that can drift out of sync.
 *
 * @par Customization
 * Double-clicking a shortcut cell (or selecting a row and clicking
 * **Edit Shortcut…**) opens a small capture dialog. The new binding is
 * rejected if it's already used by another action. **Reset** restores the
 * selected row's default; **Reset All** restores every shortcut.
 *
 * @par Printing
 * **Print…** renders the current table as a simple HTML document and sends
 * it to the system print dialog — a print-friendly cheat sheet.
 */
class ShortcutsDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit ShortcutsDialog(QWidget* parent = nullptr);
    ~ShortcutsDialog() override;

  private slots:
    void HandleCellDoubleClicked(int row, int column);
    void HandleEditSelected();
    void HandleResetSelected();
    void HandleResetAll();
    void HandlePrint();

  private:
    void setupUI();
    void populateShortcuts();
    void EditRow(int row);

    QTableWidget* m_table       {nullptr};
    QPushButton*  m_editBtn     {nullptr};
    QPushButton*  m_resetBtn    {nullptr};

    /// Shortcut id for each table row, parallel to m_table's rows. Rows for
    /// non-editable reference entries (e.g. standard "Copy") hold an empty id.
    std::vector<std::string> m_rowIds;
};

}  // namespace ui::qt
