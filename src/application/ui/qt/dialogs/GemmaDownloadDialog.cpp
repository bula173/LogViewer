#include "GemmaDownloadDialog.hpp"

#include "GemmaInferenceEngine.hpp"
#include "Logger.hpp"

#include <filesystem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QMessageBox>

namespace ui::qt
{

GemmaDownloadDialog::GemmaDownloadDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Download Gemma 2B Model"));
    setModal(true);
    setMinimumWidth(500);
    setMinimumHeight(300);

    auto layout = new QVBoxLayout(this);

    // Title
    auto titleLabel = new QLabel(tr("<b>Gemma 2B Inference Model</b>"));
    layout->addWidget(titleLabel);

    // Status
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    // Size info
    m_sizeLabel = new QLabel(tr("Model size: ~1.5 GB"));
    layout->addWidget(m_sizeLabel);

    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);

    // Buttons layout
    auto buttonLayout = new QHBoxLayout();

    m_downloadBtn = new QPushButton(tr("Download Model"));
    connect(m_downloadBtn, &QPushButton::clicked, this, &GemmaDownloadDialog::OnDownloadClicked);
    buttonLayout->addWidget(m_downloadBtn);

    m_openUrlBtn = new QPushButton(tr("Open HuggingFace"));
    connect(m_openUrlBtn, &QPushButton::clicked, this, &GemmaDownloadDialog::OnOpenUrlClicked);
    buttonLayout->addWidget(m_openUrlBtn);

    m_openFolderBtn = new QPushButton(tr("Open Models Folder"));
    connect(m_openFolderBtn, &QPushButton::clicked, this, &GemmaDownloadDialog::OnOpenFolderClicked);
    buttonLayout->addWidget(m_openFolderBtn);

    buttonLayout->addStretch();

    m_closeBtn = new QPushButton(tr("Close"));
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_closeBtn);

    layout->addLayout(buttonLayout);
    layout->addStretch();

    UpdateUI();
}

GemmaDownloadDialog::~GemmaDownloadDialog()
{
    if (m_downloadThread)
    {
        m_downloadThread->quit();
        m_downloadThread->wait();
    }
}

void GemmaDownloadDialog::UpdateUI()
{
    m_modelAvailable = ai::GemmaInferenceEngine::HasModel();

    if (m_modelAvailable)
    {
        m_statusLabel->setText(
            tr("✓ Gemma 2B model is downloaded and ready!\n\n"
               "You can now use AI-powered actor discovery in your logs."));
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
        m_downloadBtn->setEnabled(false);
        m_downloadBtn->setText(tr("Already Downloaded"));
    }
    else
    {
        m_statusLabel->setText(
            tr("Gemma 2B model is not yet downloaded.\n\n"
               "Download it to enable AI-powered actor discovery in logs.\n"
               "Requires curl or wget, and ~1.5 GB of disk space."));
        m_statusLabel->setStyleSheet("");
        m_downloadBtn->setEnabled(!m_downloading);
        if (!m_downloading)
            m_downloadBtn->setText(tr("Download Model"));
    }
}

void GemmaDownloadDialog::OnDownloadClicked()
{
    SetDownloading(true);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    util::Logger::Info("[UI] Starting Gemma model download...");

    // Download in a thread to avoid blocking UI
    if (!m_downloadThread)
        m_downloadThread = new QThread(this);

    // Run download
    std::string error = ai::GemmaInferenceEngine::DownloadModel();

    if (error.empty())
    {
        QMessageBox::information(this, tr("Download Complete"),
            tr("Gemma 2B model downloaded successfully!\n\n"
               "You can now use AI-powered actor discovery."));
    }
    else
    {
        QMessageBox::warning(this, tr("Download Failed"),
            tr("Failed to download model:\n\n") + QString::fromStdString(error));
    }

    SetDownloading(false);
    UpdateUI();
}

void GemmaDownloadDialog::OnOpenUrlClicked()
{
    const QString url = QString::fromStdString(ai::GemmaInferenceEngine::GetModelDownloadUrl());
    QDesktopServices::openUrl(QUrl(url));
}

void GemmaDownloadDialog::OnOpenFolderClicked()
{
    const auto& configPath = ai::GemmaInferenceEngine::GetModelPath();
    const auto folderPath = std::filesystem::path(configPath).parent_path();

    // Create folder if it doesn't exist
    std::filesystem::create_directories(folderPath);

    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(folderPath.string())));
}

void GemmaDownloadDialog::SetDownloading(bool downloading)
{
    m_downloading = downloading;
    m_downloadBtn->setEnabled(!downloading);
    m_openUrlBtn->setEnabled(!downloading);
    m_openFolderBtn->setEnabled(!downloading);
    m_closeBtn->setEnabled(!downloading);
    m_progressBar->setVisible(downloading);
}

void GemmaDownloadDialog::OnDownloadFinished(const QString& error)
{
    SetDownloading(false);
    if (error.isEmpty())
    {
        m_statusLabel->setText(tr("✓ Download complete!"));
        m_statusLabel->setStyleSheet("color: green;");
    }
    else
    {
        m_statusLabel->setText(tr("✗ Download failed: ") + error);
        m_statusLabel->setStyleSheet("color: red;");
    }
}

void GemmaDownloadDialog::OnDownloadProgress(int current, int total)
{
    if (total > 0)
    {
        m_progressBar->setMaximum(total);
        m_progressBar->setValue(current);
    }
}

} // namespace ui::qt
