#pragma once

#include <QDialog>
#include <memory>

class QPlainTextEdit;
class QPushButton;

namespace ui::qt
{

class ConfigEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigEditorDialog(QWidget* parent = nullptr);

private slots:
    void OnSaveClicked();
    void OnReloadClicked();
    void OnCloseClicked();

private:
    void LoadConfig();
    bool SaveConfig();

    QPlainTextEdit* m_editor {nullptr};
    QPushButton* m_saveButton {nullptr};
    QPushButton* m_reloadButton {nullptr};
    QPushButton* m_closeButton {nullptr};
};

} // namespace ui::qt