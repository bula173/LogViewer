#include "OllamaSetupDialog.hpp"
//#include "OllamaClient.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDesktopServices>
#include <QUrl>
#include <QGroupBox>
#include <QProcess>
#include <QFile>
#include <QDir>

namespace ui::qt
{

OllamaSetupDialog::OllamaSetupDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("AI Analysis Setup"));
    setMinimumWidth(500);
    BuildUi();
    UpdateStatus();
}

bool OllamaSetupDialog::CheckOllamaInstalled()
{
#ifdef _WIN32
    // Windows paths
    const QStringList paths = {
        "C:/Program Files/Ollama/ollama.exe",
        "C:/Program Files (x86)/Ollama/ollama.exe",
        QDir::homePath() + "/AppData/Local/Programs/Ollama/ollama.exe",
        QDir::homePath() + "/AppData/Local/Ollama/ollama.exe"
    };
#else
    // macOS/Linux paths
    const QStringList paths = {
        "/usr/local/bin/ollama",
        "/opt/homebrew/bin/ollama",
        "/usr/bin/ollama",
        QDir::homePath() + "/.ollama/bin/ollama"
    };
#endif

    for (const auto& path : paths)
    {
        if (QFile::exists(path))
            return true;
    }

    // Try to find in PATH
    QProcess process;
#ifdef _WIN32
    process.start("where", QStringList() << "ollama");
#else
    process.start("which", QStringList() << "ollama");
#endif
    process.waitForFinished(1000);
    return process.exitCode() == 0;
}

bool OllamaSetupDialog::CheckOllamaRunning()
{
    // TODO: Implement with OllamaClient
    return false;
}

void OllamaSetupDialog::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Info section
    auto* infoGroup = new QGroupBox(tr("About AI Analysis"), this);
    auto* infoLayout = new QVBoxLayout(infoGroup);
    
    auto* infoText = new QLabel(
        tr("<p>LogViewer uses <b>Ollama</b> for local AI-powered log analysis.</p>"
           "<p>Benefits:</p>"
           "<ul>"
           "<li>✓ Runs completely on your machine</li>"
           "<li>✓ No data sent to cloud services</li>"
           "<li>✓ Fast analysis (15-30 seconds)</li>"
           "<li>✓ Free and open source</li>"
           "</ul>"), this);
    infoText->setWordWrap(true);
    infoLayout->addWidget(infoText);
    
    mainLayout->addWidget(infoGroup);

    // Status section
    auto* statusGroup = new QGroupBox(tr("Installation Status"), this);
    auto* statusLayout = new QVBoxLayout(statusGroup);
    
    m_statusLabel = new QLabel(tr("Checking..."), this);
    m_statusLabel->setWordWrap(true);
    statusLayout->addWidget(m_statusLabel);
    
    mainLayout->addWidget(statusGroup);

    // Installation instructions
    auto* instructionsGroup = new QGroupBox(tr("Installation Steps"), this);
    auto* instructionsLayout = new QVBoxLayout(instructionsGroup);
    
#ifdef _WIN32
    auto* steps = new QLabel(
        tr("<p><b>1. Install Ollama</b></p>"
           "<p>Download installer from <b>ollama.ai</b></p>"
           "<p><b>2. Start Ollama</b></p>"
           "<p>Ollama runs automatically as a Windows service after installation</p>"
           "<p><b>3. Download Model(s)</b></p>"
           "<p>Open Command Prompt and run: <code>ollama pull qwen2.5-coder:7b</code></p>"
           "<p>Or any model shown in the AI Analysis panel dropdown</p>"
           "<p><b>4. Click 'Check Status' below</b></p>"), this);
#else
    auto* steps = new QLabel(
        tr("<p><b>1. Install Ollama</b></p>"
           "<p>macOS: <code>brew install ollama</code><br/>"
           "Linux: <code>curl -fsSL https://ollama.ai/install.sh | sh</code><br/>"
           "or download from <b>ollama.ai</b></p>"
           "<p><b>2. Start Ollama</b></p>"
           "<p>Run in terminal: <code>ollama serve</code></p>"
           "<p><b>3. Download Model(s)</b></p>"
           "<p>Recommended: <code>ollama pull qwen2.5-coder:7b</code></p>"
           "<p>Or any model shown in the AI Analysis panel dropdown</p>"
           "<p><b>4. Click 'Check Status' below</b></p>"), this);
#endif
    steps->setWordWrap(true);
    steps->setTextFormat(Qt::RichText);
    instructionsLayout->addWidget(steps);
    
    mainLayout->addWidget(instructionsGroup);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    
    m_downloadButton = new QPushButton(tr("Open Ollama Website"), this);
    m_downloadButton->setToolTip(tr("Opens https://ollama.ai in your browser"));
    connect(m_downloadButton, &QPushButton::clicked,
            this, &OllamaSetupDialog::OnOpenDownloadPage);
    
    m_checkButton = new QPushButton(tr("Check Status"), this);
    m_checkButton->setToolTip(tr("Verify Ollama installation and availability"));
    connect(m_checkButton, &QPushButton::clicked,
            this, &OllamaSetupDialog::OnCheckStatus);
    
    m_closeButton = new QPushButton(tr("Close"), this);
    connect(m_closeButton, &QPushButton::clicked,
            this, &QDialog::accept);
    
    buttonLayout->addWidget(m_downloadButton);
    buttonLayout->addWidget(m_checkButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
}

void OllamaSetupDialog::OnOpenDownloadPage()
{
    QDesktopServices::openUrl(QUrl("https://ollama.ai"));
}

void OllamaSetupDialog::OnCheckStatus()
{
    UpdateStatus();
}

void OllamaSetupDialog::UpdateStatus()
{
    const bool installed = CheckOllamaInstalled();
    const bool running = CheckOllamaRunning();

    QString statusHtml;
    
    if (!installed)
    {
        statusHtml = tr("<p style='color: red;'><b>⚠ Ollama Not Installed</b></p>"
                       "<p>Please install Ollama to use AI analysis features.</p>");
        m_statusLabel->setStyleSheet("QLabel { background-color: #ffe6e6; padding: 10px; }");
    }
    else if (!running)
    {
#ifdef _WIN32
        statusHtml = tr("<p style='color: orange;'><b>⚠ Ollama Not Running</b></p>"
                       "<p>Ollama is installed but the service is not running.</p>"
                       "<p>Restart the Ollama service from Windows Services or reinstall Ollama.</p>");
#else
        statusHtml = tr("<p style='color: orange;'><b>⚠ Ollama Not Running</b></p>"
                       "<p>Ollama is installed but the service is not running.</p>"
                       "<p>Start it with: <code>ollama serve</code></p>");
#endif
        m_statusLabel->setStyleSheet("QLabel { background-color: #fff3cd; padding: 10px; }");
    }
    else
    {
        statusHtml = tr("<p style='color: green;'><b>✓ Ollama Ready</b></p>"
                       "<p>AI analysis is ready to use!</p>");
        m_statusLabel->setStyleSheet("QLabel { background-color: #d4edda; padding: 10px; }");
    }

    m_statusLabel->setText(statusHtml);
    m_statusLabel->setTextFormat(Qt::RichText);
}

} // namespace ui::qt
