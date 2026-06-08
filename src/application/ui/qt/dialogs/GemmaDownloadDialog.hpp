#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QThread>

namespace ui::qt
{

/// Dialog for downloading Gemma 2B model
class GemmaDownloadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GemmaDownloadDialog(QWidget* parent = nullptr);
    ~GemmaDownloadDialog() override;

    /// Returns true if model is available (either already downloaded or just downloaded)
    bool ModelAvailable() const { return m_modelAvailable; }

private slots:
    void OnDownloadClicked();
    void OnOpenUrlClicked();
    void OnOpenFolderClicked();
    void OnDownloadFinished(const QString& error);
    void OnDownloadProgress(int current, int total);

private:
    void UpdateUI();
    void SetDownloading(bool downloading);

    QLabel* m_statusLabel {nullptr};
    QLabel* m_sizeLabel {nullptr};
    QPushButton* m_downloadBtn {nullptr};
    QPushButton* m_openUrlBtn {nullptr};
    QPushButton* m_openFolderBtn {nullptr};
    QPushButton* m_closeBtn {nullptr};
    QProgressBar* m_progressBar {nullptr};
    QThread* m_downloadThread {nullptr};

    bool m_modelAvailable {false};
    bool m_downloading {false};
};

} // namespace ui::qt
