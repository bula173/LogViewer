#include "AIAnalysisPanel.hpp"
#include "AIConfigPanel.hpp"
#include "ui/qt/EventsTableView.hpp"
#include "OllamaClient.hpp"
#include "OllamaSetupDialog.hpp"
#include "util/Logger.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QCoreApplication>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

namespace ui::qt
{

AIAnalysisPanel::AIAnalysisPanel(std::shared_ptr<ai::AIServiceWrapper> aiService,
                                 std::shared_ptr<ai::LogAnalyzer> analyzer,
                                 EventsTableView* eventsView,
                                 QWidget* parent)
    : QWidget(parent)
    , m_aiService(aiService)
    , m_analyzer(analyzer)
    , m_eventsView(eventsView)
{
    BuildUi();
}

void AIAnalysisPanel::SetConfigPanel(AIConfigPanel* configPanel)
{
    m_configPanel = configPanel;
}

void AIAnalysisPanel::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Prompt configuration group
    auto* promptGroup = new QGroupBox(tr("Prompt Configuration"), this);
    auto* promptLayout = new QFormLayout(promptGroup);
    promptLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // Custom prompt option
    m_useCustomPromptCheckbox = new QCheckBox(tr("Use Custom Prompt"), this);
    m_useCustomPromptCheckbox->setToolTip(tr("Enable to write your own analysis prompt"));
    connect(m_useCustomPromptCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        m_customPromptEdit->setEnabled(checked);
        m_predefinedPromptCombo->setEnabled(checked);
        m_loadPromptButton->setEnabled(checked);
        m_savePromptButton->setEnabled(checked);
    });
    promptLayout->addRow(m_useCustomPromptCheckbox);

    // Predefined prompts selector
    auto* promptSelectorLayout = new QHBoxLayout();
    m_predefinedPromptCombo = new QComboBox(this);
    m_predefinedPromptCombo->setEnabled(false);
    m_predefinedPromptCombo->setToolTip(tr("Select a predefined prompt template"));
    connect(m_predefinedPromptCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AIAnalysisPanel::OnPredefinedPromptSelected);
    promptSelectorLayout->addWidget(m_predefinedPromptCombo, 1);
    
    m_loadPromptButton = new QPushButton(tr("Load..."), this);
    m_loadPromptButton->setEnabled(false);
    m_loadPromptButton->setToolTip(tr("Load prompt from a text file"));
    connect(m_loadPromptButton, &QPushButton::clicked,
            this, &AIAnalysisPanel::OnLoadPromptFile);
    promptSelectorLayout->addWidget(m_loadPromptButton);
    
    m_savePromptButton = new QPushButton(tr("Save..."), this);
    m_savePromptButton->setEnabled(false);
    m_savePromptButton->setToolTip(tr("Save current prompt to a file"));
    connect(m_savePromptButton, &QPushButton::clicked,
            this, &AIAnalysisPanel::OnSavePromptFile);
    promptSelectorLayout->addWidget(m_savePromptButton);
    
    promptLayout->addRow(tr("Prompt Template:"), promptSelectorLayout);
    
    // Load predefined prompts
    LoadPredefinedPrompts();

    // Custom prompt editor
    m_customPromptEdit = new QTextEdit(this);
    m_customPromptEdit->setPlaceholderText(tr("Enter your custom analysis prompt...\n\nExamples:\n- Find all database connection errors\n- What performance issues exist?\n- Analyze authentication failures"));
    m_customPromptEdit->setEnabled(false);
    m_customPromptEdit->setMinimumHeight(100);
    m_customPromptEdit->setMaximumHeight(150);
    m_customPromptEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_customPromptEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_customPromptEdit->setToolTip(tr("Custom prompt will be used instead of predefined analysis types"));
    promptLayout->addRow(tr("Custom Prompt:"), m_customPromptEdit);

    // Analyze button
    m_analyzeButton = new QPushButton(tr("Analyze Logs"), this);
    m_analyzeButton->setToolTip(tr("Run AI analysis on current log data"));
    connect(m_analyzeButton, &QPushButton::clicked, 
            this, &AIAnalysisPanel::OnAnalyzeClicked);
    
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_analyzeButton);
    promptLayout->addRow(buttonLayout);

    mainLayout->addWidget(promptGroup);

    // Results display
    auto* resultsGroup = new QGroupBox(tr("Analysis Results"), this);
    auto* resultsLayout = new QVBoxLayout(resultsGroup);
    
    m_resultsText = new QTextEdit(this);
    m_resultsText->setReadOnly(true);
    m_resultsText->setPlaceholderText(
        tr("Analysis results will appear here.\n\n"
           "Note: Requires AI service to be configured.\n"
           "Check configuration panel to set up provider and model."));
    resultsLayout->addWidget(m_resultsText);

    mainLayout->addWidget(resultsGroup, 1);
}

void AIAnalysisPanel::OnAnalyzeClicked()
{
    if (!m_analyzer || !m_analyzer.get())
    {
        m_resultsText->setPlainText(tr("Error: Analyzer not initialized"));
        return;
    }

    if (!m_configPanel)
    {
        m_resultsText->setPlainText(tr("Error: Configuration panel not connected"));
        return;
    }

    m_analyzeButton->setEnabled(false);
    
    // Get current provider and model info from AIService
    QString providerInfo;
    if (m_aiService && m_aiService->service)
    {
        const std::string provider = m_aiService->service->GetProviderName();
        const std::string model = m_aiService->service->GetModelName();
        
        providerInfo = QString("Provider: %1").arg(QString::fromStdString(provider));
        if (!model.empty())
        {
            providerInfo += QString(" | Model: %1").arg(QString::fromStdString(model));
        }
    }
    else
    {
        providerInfo = tr("AI Service not initialized");
    }
    
    m_resultsText->setPlainText(tr("Processing, please wait...\n\n%1").arg(providerInfo));
    
    emit AnalysisStarted();

    const size_t maxEvents = static_cast<size_t>(m_configPanel->GetMaxEvents());
    
    // Get filtered indices if any filters are active
    const std::vector<unsigned long>* filteredIndices = 
        m_eventsView ? m_eventsView->GetFilteredIndices() : nullptr;
    
    // Check if using custom prompt
    const bool useCustomPrompt = m_useCustomPromptCheckbox->isChecked();
    const QString customPrompt = m_customPromptEdit->toPlainText().trimmed();
    
    if (useCustomPrompt && customPrompt.isEmpty())
    {
        m_resultsText->setPlainText(tr("Error: Custom prompt cannot be empty"));
        m_analyzeButton->setEnabled(true);
        return;
    }

    // Get analysis type from config panel
    const ai::LogAnalyzer::AnalysisType analysisType = m_configPanel->GetAnalysisType();

    // Run analysis asynchronously to avoid blocking UI
    auto future = QtConcurrent::run([this, useCustomPrompt, customPrompt, analysisType, maxEvents, filteredIndices]() -> std::string {
        try
        {
            if (useCustomPrompt)
            {
                util::Logger::Info("Starting custom AI analysis: maxEvents={}, filtered={}", 
                    maxEvents, filteredIndices != nullptr);
                return m_analyzer->AnalyzeWithCustomPrompt(customPrompt.toStdString(), maxEvents, filteredIndices);
            }
            else
            {
                util::Logger::Info("Starting AI analysis: type={}, maxEvents={}, filtered={}",
                    ai::LogAnalyzer::GetAnalysisTypeName(analysisType), maxEvents, filteredIndices != nullptr);
                return m_analyzer->Analyze(analysisType, maxEvents, filteredIndices);
            }
        }
        catch (const std::exception& e)
        {
            util::Logger::Error("AI analysis failed: {}", e.what());
            throw;
        }
    });

    auto* watcher = new QFutureWatcher<std::string>(this);
    connect(watcher, &QFutureWatcher<std::string>::finished, this, [this, watcher]() {
        try
        {
            const std::string result = watcher->result();
            m_resultsText->setPlainText(QString::fromStdString(result));
            emit AnalysisCompleted(QString::fromStdString(result));
        }
        catch (const std::exception& e)
        {
            const QString error = tr("Analysis failed: %1").arg(e.what());
            m_resultsText->setPlainText(error);
        }
        
        m_analyzeButton->setEnabled(true);
        watcher->deleteLater();
    });
    
    watcher->setFuture(future);
}

void AIAnalysisPanel::OnSetupClicked()
{
    OllamaSetupDialog dlg(qobject_cast<QWidget*>(parent()));
    dlg.exec();
}

void AIAnalysisPanel::LoadPredefinedPrompts()
{
    m_predefinedPrompts.clear();
    m_predefinedPromptCombo->clear();
    m_predefinedPromptCombo->addItem(tr("-- Select Template --"), "");
    
    // Try multiple possible locations for prompts directory
    QStringList searchPaths;
    searchPaths << QDir::currentPath() + "/etc/prompts";  // Working directory
    searchPaths << QCoreApplication::applicationDirPath() + "/etc/prompts";  // Application directory
    
#ifdef Q_OS_MAC
    // macOS bundle: Contents/Resources/etc/prompts
    searchPaths << QCoreApplication::applicationDirPath() + "/../Resources/etc/prompts";
#endif
    
    QString promptsDir;
    QDir dir;
    
    // Find the first existing prompts directory
    for (const QString& path : searchPaths)
    {
        dir.setPath(path);
        if (dir.exists())
        {
            promptsDir = path;
            util::Logger::Info("Found prompts directory: {}", promptsDir.toStdString());
            break;
        }
    }
    
    if (promptsDir.isEmpty())
    {
        util::Logger::Info("No prompts directory found in search paths");
        
        // Add some default built-in prompts as fallback
        m_predefinedPrompts[tr("Performance Analysis")] = 
            tr("Analyze performance-related issues in the logs. Look for:\n"
               "- Slow operations or timeouts\n"
               "- Resource consumption patterns\n"
               "- Bottlenecks and their causes\n"
               "- Recommendations for optimization");
        
        m_predefinedPrompts[tr("Security Audit")] = 
            tr("Perform a security-focused analysis. Identify:\n"
               "- Authentication failures or suspicious login attempts\n"
               "- Authorization issues or privilege escalations\n"
               "- Potential security vulnerabilities\n"
               "- Unusual access patterns");
        
        m_predefinedPrompts[tr("Database Issues")] = 
            tr("Focus on database-related problems:\n"
               "- Connection failures or timeouts\n"
               "- Query errors or slow queries\n"
               "- Transaction issues\n"
               "- Data integrity problems");
               
        m_predefinedPrompts[tr("Network Diagnostics")] = 
            tr("Analyze network-related events:\n"
               "- Connection failures\n"
               "- Timeouts and latency issues\n"
               "- Protocol errors\n"
               "- Network topology problems");
    }
    else
    {
        // Load prompts from files
        const QStringList filters{"*.txt", "*.prompt"};
        const QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);
        
        for (const QFileInfo& fileInfo : files)
        {
            const QString content = LoadPromptFromFile(fileInfo.absoluteFilePath());
            if (!content.isEmpty())
            {
                const QString name = fileInfo.completeBaseName();
                m_predefinedPrompts[name] = content;
                util::Logger::Info("Loaded prompt template: {}", name.toStdString());
            }
        }
    }
    
    // Populate combo box
    for (auto it = m_predefinedPrompts.constBegin(); it != m_predefinedPrompts.constEnd(); ++it)
    {
        m_predefinedPromptCombo->addItem(it.key(), it.key());
    }
    
    util::Logger::Info("Loaded {} predefined prompt templates", m_predefinedPrompts.size());
}

QString AIAnalysisPanel::LoadPromptFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        util::Logger::Error("Failed to open prompt file: {}", filePath.toStdString());
        return QString();
    }
    
    const QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    return content.trimmed();
}

void AIAnalysisPanel::OnPredefinedPromptSelected(int index)
{
    if (index <= 0 || !m_customPromptEdit)
        return;
    
    const QString templateName = m_predefinedPromptCombo->currentData().toString();
    if (m_predefinedPrompts.contains(templateName))
    {
        m_customPromptEdit->setPlainText(m_predefinedPrompts[templateName]);
        util::Logger::Info("Loaded predefined prompt: {}", templateName.toStdString());
    }
}

void AIAnalysisPanel::OnLoadPromptFile()
{
    QFileDialog dialog(this, tr("Load Prompt from File"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    dialog.setDirectory(QCoreApplication::applicationDirPath() + "/etc/prompts");
    dialog.setNameFilter(tr("Prompt Files (*.txt *.prompt);;All Files (*)"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    
    if (dialog.exec() != QDialog::Accepted)
        return;
    
    const QString fileName = dialog.selectedFiles().value(0);
    
    if (fileName.isEmpty())
        return;
    
    const QString content = LoadPromptFromFile(fileName);
    if (!content.isEmpty())
    {
        m_customPromptEdit->setPlainText(content);
        util::Logger::Info("Loaded prompt from file: {}", fileName.toStdString());
    }
    else
    {
        QMessageBox::warning(this, tr("Load Failed"),
            tr("Failed to load prompt from file:\n%1").arg(fileName));
    }
}

void AIAnalysisPanel::OnSavePromptFile()
{
    const QString currentPrompt = m_customPromptEdit->toPlainText().trimmed();
    
    if (currentPrompt.isEmpty())
    {
        QMessageBox::information(this, tr("No Prompt"),
            tr("The custom prompt is empty. Nothing to save."));
        return;
    }
    
    QFileDialog dialog(this, tr("Save Prompt to File"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    dialog.setDirectory(QCoreApplication::applicationDirPath() + "/etc/prompts");
    dialog.setNameFilter(tr("Text Files (*.txt);;Prompt Files (*.prompt);;All Files (*)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.selectFile("my_prompt.txt");
    
    if (dialog.exec() != QDialog::Accepted)
        return;
    
    const QString fileName = dialog.selectedFiles().value(0);
    
    if (fileName.isEmpty())
        return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Save Failed"),
            tr("Failed to save prompt to file:\n%1\n\nError: %2")
                .arg(fileName)
                .arg(file.errorString()));
        util::Logger::Error("Failed to save prompt file: {}", fileName.toStdString());
        return;
    }
    
    QTextStream out(&file);
    out << currentPrompt;
    file.close();
    
    util::Logger::Info("Saved prompt to file: {}", fileName.toStdString());
    
    QMessageBox::information(this, tr("Prompt Saved"),
        tr("Prompt successfully saved to:\n%1\n\n"
           "You can load it later using the \"Load...\" button or by placing it in the etc/prompts directory.")
            .arg(fileName));
}

} // namespace ui::qt
