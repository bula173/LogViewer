#pragma once

#include <QDialog>

class QPushButton;
class QLabel;

namespace ui::qt
{

/**
 * @brief Dialog to guide users through Ollama installation
 */
class OllamaSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OllamaSetupDialog(QWidget* parent = nullptr);

    static bool CheckOllamaInstalled();
    static bool CheckOllamaRunning();

private slots:
    void OnOpenDownloadPage();
    void OnCheckStatus();

private:
    void BuildUi();
    void UpdateStatus();

    QLabel* m_statusLabel{nullptr};
    QPushButton* m_downloadButton{nullptr};
    QPushButton* m_checkButton{nullptr};
    QPushButton* m_closeButton{nullptr};
};

} // namespace ui::qt
