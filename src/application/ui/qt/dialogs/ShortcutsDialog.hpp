#pragma once

#include <QDialog>

class QTableWidget;

namespace ui::qt
{

/**
 * @brief Dialog showing all keyboard shortcuts available in the application.
 *
 * Organized by category (File, Edit, View, Tools, Help).
 * Provides quick reference for users.
 */
class ShortcutsDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit ShortcutsDialog(QWidget* parent = nullptr);
    ~ShortcutsDialog() override;

  private:
    void setupUI();
    void populateShortcuts();
    void addShortcut(const QString& action, const QString& shortcut, const QString& category);

    QTableWidget* m_table {nullptr};
};

}  // namespace ui::qt
