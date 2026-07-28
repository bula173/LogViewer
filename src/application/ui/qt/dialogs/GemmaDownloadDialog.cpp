#include "GemmaDownloadDialog.hpp"

#include "GemmaInferenceEngine.hpp"
#include "Logger.hpp"
#include "Config.hpp"

#include <filesystem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QMessageBox>
#include <QLineEdit>
#include <QComboBox>

namespace ui::qt
{

GemmaDownloadDialog::GemmaDownloadDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Download AI Model"));
    setModal(true);
    setMinimumWidth(550);
    setMinimumHeight(350);

    auto layout = new QVBoxLayout(this);

    // Title
    auto titleLabel = new QLabel(tr("<b>Download Local AI Model for Analysis</b>"));
    layout->addWidget(titleLabel);

    // Model type selection
    auto typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel(tr("Model Type:")));
    m_modelTypeCombo = new QComboBox();
    LoadModelPresets();
    connect(m_modelTypeCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &GemmaDownloadDialog::OnModelTypeChanged);
    typeLayout->addWidget(m_modelTypeCombo);
    layout->addLayout(typeLayout);

    // Model URL
    auto urlLayout = new QHBoxLayout();
    urlLayout->addWidget(new QLabel(tr("Model URL:")));
    m_modelUrlEdit = new QLineEdit();
    m_modelUrlEdit->setPlaceholderText(tr("https://huggingface.co/..."));
    urlLayout->addWidget(m_modelUrlEdit);
    layout->addLayout(urlLayout);

    // Model filename
    auto nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(tr("Save As:")));
    m_modelNameEdit = new QLineEdit();
    m_modelNameEdit->setPlaceholderText(tr("model-name.gguf"));
    nameLayout->addWidget(m_modelNameEdit);
    layout->addLayout(nameLayout);

    // Status
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    // Size info
    m_sizeLabel = new QLabel();
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

    OnModelTypeChanged(0);  // Initialize with first preset
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
            tr("✓ AI model is downloaded and ready!\n\n"
               "You can now use AI-powered actor discovery in your logs."));
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
        m_downloadBtn->setEnabled(false);
        m_downloadBtn->setText(tr("Already Downloaded"));
    }
    else
    {
        m_statusLabel->setText(
            tr("No AI model is downloaded yet.\n\n"
               "Download a model to enable AI-powered actor discovery in logs.\n"
               "Requires curl or wget."));
        m_statusLabel->setStyleSheet("");
        m_downloadBtn->setEnabled(!m_downloading);
        if (!m_downloading)
            m_downloadBtn->setText(tr("Download Model"));
    }
}

void GemmaDownloadDialog::OnDownloadClicked()
{
    const QString url = m_modelUrlEdit->text();
    const QString filename = m_modelNameEdit->text();

    // Validate inputs
    if (url.isEmpty())
    {
        QMessageBox::warning(this, tr("Invalid Input"),
            tr("Please enter a model URL."));
        return;
    }

    if (filename.isEmpty())
    {
        QMessageBox::warning(this, tr("Invalid Input"),
            tr("Please enter a filename."));
        return;
    }

    if (!filename.endsWith(".gguf"))
    {
        QMessageBox::warning(this, tr("Invalid Filename"),
            tr("Filename must end with .gguf"));
        return;
    }

    SetDownloading(true);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    util::Logger::Info("[UI] Starting model download: {}", filename.toStdString());
    util::Logger::Info("[UI] URL: {}", url.toStdString());

    // Run download with selected URL and filename
    std::string error = ai::GemmaInferenceEngine::DownloadModel(
        url.toStdString(),
        filename.toStdString()
    );

    if (error.empty())
    {
        QMessageBox::information(this, tr("Download Complete"),
            tr("Model downloaded successfully!\n\n") +
            filename + tr("\n\nYou can now use AI-powered actor discovery."));
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
    auto modelPath = ai::GemmaInferenceEngine::GetModelPath();

    // If model path is empty, compute default
    if (modelPath.empty())
    {
        const auto& configPath = config::GetConfig().GetConfigFilePath();
        const auto appDir = std::filesystem::path(configPath).parent_path();
        modelPath = (appDir / "models").string();
    }

    const auto folderPath = std::filesystem::path(modelPath).parent_path();

    // Create folder if it doesn't exist
    if (!folderPath.empty())
    {
        try
        {
            std::filesystem::create_directories(folderPath);
        }
        catch (const std::exception& e)
        {
            util::Logger::Warn("[UI] Failed to create folder: {}", e.what());
        }
    }

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

void GemmaDownloadDialog::OnModelTypeChanged(int index)
{
    // Preset models - direct download URLs from HuggingFace (freely available)
    // Format: Model Name\tURL\tFilename\tSize(GB)
    const QStringList models = {
        "TinyLlama 1.1B (Recommended)\thttps://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf\ttinyllama-1.1b.gguf\t0.7",
        "Phi 2 2.7B\thttps://huggingface.co/TheBloke/phi-2-GGUF/resolve/main/phi-2.Q4_K_M.gguf\tphi-2-q4.gguf\t1.6",
        "Neural Chat 7B\thttps://huggingface.co/TheBloke/neural-chat-7B-v3-2-GGUF/resolve/main/neural-chat-7b-v3-2.Q4_K_M.gguf\tneural-chat-7b.gguf\t4.2",
        "Custom Model\thttps://huggingface.co/TheBloke/MODEL-NAME-GGUF/resolve/main/MODEL.gguf\tcustom.gguf\t0.0"
    };

    if (index >= 0 && index < models.size())
    {
        const QStringList parts = models[index].split('\t');
        if (parts.size() >= 4)
        {
            m_modelUrlEdit->setText(parts[1]);
            m_modelNameEdit->setText(parts[2]);
            m_sizeLabel->setText(tr("Model size: ~%1 GB").arg(parts[3]));
        }
    }
}

void GemmaDownloadDialog::LoadModelPresets()
{
    m_modelTypeCombo->addItem(tr("TinyLlama 1.1B (Recommended)"));
    m_modelTypeCombo->addItem(tr("Phi 2 2.7B"));
    m_modelTypeCombo->addItem(tr("Neural Chat 7B"));
    m_modelTypeCombo->addItem(tr("Custom Model"));
}

void GemmaDownloadDialog::SaveModelPreferences()
{
    util::Logger::Info("[UI] Model '{}' downloaded and ready to use",
                      m_modelNameEdit->text().toStdString());
}

} // namespace ui::qt
