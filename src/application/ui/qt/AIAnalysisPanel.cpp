#include "ui/qt/AIAnalysisPanel.hpp"
#include "ui/qt/OllamaSetupDialog.hpp"
#include "ui/qt/EventsTableView.hpp"
#include "ai/OllamaClient.hpp"
#include "util/Logger.hpp"
#include "config/Config.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
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

AIAnalysisPanel::AIAnalysisPanel(std::shared_ptr<ai::IAIService> aiService,
                                 std::shared_ptr<ai::LogAnalyzer> analyzer,
                                 EventsTableView* eventsView,
                                 QWidget* parent)
    : QWidget(parent)
    , m_aiService(std::move(aiService))
    , m_analyzer(std::move(analyzer))
    , m_eventsView(eventsView)
{
    BuildUi();
    UpdateStatusLabel();
}

void AIAnalysisPanel::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Controls group
    auto* controlsGroup = new QGroupBox(tr("AI Analysis"), this);
    auto* controlsLayout = new QFormLayout(controlsGroup);
    controlsLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    controlsLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Model selector
    m_modelCombo = new QComboBox(this);
    m_modelCombo->setToolTip(tr("Select AI model for analysis. Larger models give better results but are slower."));
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AIAnalysisPanel::OnModelChanged);
    
    // Populate with installed models
    PopulateModelList();
    
    auto* modelLayout = new QHBoxLayout();
    modelLayout->addWidget(m_modelCombo, 1);
    
    auto* refreshModelsBtn = new QPushButton(tr("Refresh"), this);
    refreshModelsBtn->setToolTip(tr("Refresh list of installed models"));
    connect(refreshModelsBtn, &QPushButton::clicked,
            this, &AIAnalysisPanel::OnRefreshModels);
    modelLayout->addWidget(refreshModelsBtn);
    
    auto* showModelsBtn = new QPushButton(tr("Show Details"), this);
    showModelsBtn->setToolTip(tr("Show detailed information about installed models"));
    connect(showModelsBtn, &QPushButton::clicked,
            this, &AIAnalysisPanel::OnShowInstalledModels);
    modelLayout->addWidget(showModelsBtn);
    
    controlsLayout->addRow(tr("Model:"), modelLayout);

    // Analysis type selector
    m_analysisTypeCombo = new QComboBox(this);
    m_analysisTypeCombo->addItem(tr("Summary"), 
        static_cast<int>(ai::LogAnalyzer::AnalysisType::Summary));
    m_analysisTypeCombo->addItem(tr("Error Analysis"), 
        static_cast<int>(ai::LogAnalyzer::AnalysisType::ErrorAnalysis));
    m_analysisTypeCombo->addItem(tr("Pattern Detection"), 
        static_cast<int>(ai::LogAnalyzer::AnalysisType::PatternDetection));
    m_analysisTypeCombo->addItem(tr("Root Cause Analysis"), 
        static_cast<int>(ai::LogAnalyzer::AnalysisType::RootCause));
    m_analysisTypeCombo->addItem(tr("Timeline"), 
        static_cast<int>(ai::LogAnalyzer::AnalysisType::Timeline));
    controlsLayout->addRow(tr("Analysis Type:"), m_analysisTypeCombo);

    // Custom prompt option
    m_useCustomPromptCheckbox = new QCheckBox(tr("Use Custom Prompt"), this);
    m_useCustomPromptCheckbox->setToolTip(tr("Enable to write your own analysis prompt"));
    connect(m_useCustomPromptCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        m_customPromptEdit->setEnabled(checked);
        m_analysisTypeCombo->setEnabled(!checked);
        m_predefinedPromptCombo->setEnabled(checked);
        m_loadPromptButton->setEnabled(checked);
        m_savePromptButton->setEnabled(checked);
    });
    controlsLayout->addRow(m_useCustomPromptCheckbox);

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
    
    controlsLayout->addRow(tr("Prompt Template:"), promptSelectorLayout);
    
    // Load predefined prompts
    LoadPredefinedPrompts();

    m_customPromptEdit = new QTextEdit(this);
    m_customPromptEdit->setPlaceholderText(tr("Enter your custom analysis prompt...\n\nExamples:\n- Find all database connection errors\n- What performance issues exist?\n- Analyze authentication failures"));
    m_customPromptEdit->setEnabled(false);
    m_customPromptEdit->setMinimumHeight(80);
    m_customPromptEdit->setMaximumHeight(150);
    m_customPromptEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_customPromptEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_customPromptEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_customPromptEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    m_customPromptEdit->setToolTip(tr("Custom prompt will be used instead of predefined analysis types"));
    controlsLayout->addRow(tr("Custom Prompt:"), m_customPromptEdit);

    // Max events selector
    m_maxEventsSpin = new QSpinBox(this);
    m_maxEventsSpin->setMinimum(10);
    m_maxEventsSpin->setMaximum(1000);
    m_maxEventsSpin->setValue(100);
    m_maxEventsSpin->setSingleStep(10);
    m_maxEventsSpin->setToolTip(tr("Maximum number of events to analyze"));
    controlsLayout->addRow(tr("Max Events:"), m_maxEventsSpin);

    // Status label
    m_statusLabel = new QLabel(this);
    controlsLayout->addRow(tr("Status:"), m_statusLabel);

    // Analyze button
    m_analyzeButton = new QPushButton(tr("Analyze Logs"), this);
    m_analyzeButton->setToolTip(tr("Run AI analysis on current log data"));
    connect(m_analyzeButton, &QPushButton::clicked, 
            this, &AIAnalysisPanel::OnAnalyzeClicked);
    
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_analyzeButton);
    controlsLayout->addRow(buttonLayout);

    mainLayout->addWidget(controlsGroup);

    // Results display
    auto* resultsGroup = new QGroupBox(tr("Analysis Results"), this);
    auto* resultsLayout = new QVBoxLayout(resultsGroup);
    
    m_resultsText = new QTextEdit(this);
    m_resultsText->setReadOnly(true);
    m_resultsText->setPlaceholderText(
        tr("Analysis results will appear here.\n\n"
           "Note: Requires Ollama to be installed and running.\n"
           "Click Tools → AI Analysis Setup for instructions."));
    resultsLayout->addWidget(m_resultsText);

    mainLayout->addWidget(resultsGroup, 1);
}

void AIAnalysisPanel::OnSetupClicked()
{
    OllamaSetupDialog dlg(qobject_cast<QWidget*>(parent()));
    dlg.exec();
    UpdateStatusLabel();
}

void AIAnalysisPanel::OnAnalyzeClicked()
{
    if (!m_analyzer)
    {
        m_resultsText->setPlainText(tr("Error: Analyzer not initialized"));
        return;
    }

    m_analyzeButton->setEnabled(false);
    m_statusLabel->setText(tr("Analyzing..."));
    m_resultsText->setPlainText(tr("Processing, please wait..."));
    
    emit AnalysisStarted();

    const size_t maxEvents = static_cast<size_t>(m_maxEventsSpin->value());
    
    // Get filtered indices if any filters are active
    const std::vector<unsigned long>* filteredIndices = 
        m_eventsView ? m_eventsView->GetFilteredIndices() : nullptr;
    
    // Check if using custom prompt
    const bool useCustomPrompt = m_useCustomPromptCheckbox->isChecked();
    const QString customPrompt = m_customPromptEdit->toPlainText().trimmed();
    
    if (useCustomPrompt && customPrompt.isEmpty())
    {
        m_resultsText->setPlainText(tr("Error: Custom prompt cannot be empty"));
        m_statusLabel->setText(tr("Error"));
        m_analyzeButton->setEnabled(true);
        return;
    }

    // Run analysis asynchronously to avoid blocking UI
    auto future = QtConcurrent::run([this, useCustomPrompt, customPrompt, maxEvents, filteredIndices]() -> std::string {
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
                const int typeIndex = m_analysisTypeCombo->currentData().toInt();
                const auto analysisType = static_cast<ai::LogAnalyzer::AnalysisType>(typeIndex);
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
            m_statusLabel->setText(tr("Analysis complete"));
            emit AnalysisCompleted(QString::fromStdString(result));
        }
        catch (const std::exception& e)
        {
            const QString error = tr("Analysis failed: %1").arg(e.what());
            m_resultsText->setPlainText(error);
            m_statusLabel->setText(tr("Error"));
        }
        
        m_analyzeButton->setEnabled(true);
        watcher->deleteLater();
    });
    
    watcher->setFuture(future);
}

void AIAnalysisPanel::OnModelChanged(int index)
{
    if (!m_aiService || index < 0)
        return;
    
    const QString modelName = m_modelCombo->itemData(index).toString();
    
    // Cast to OllamaClient to change model
    if (auto* ollamaClient = dynamic_cast<ai::OllamaClient*>(m_aiService.get()))
    {
        ollamaClient->SetModel(modelName.toStdString());
        util::Logger::Info("AI model changed to: {}", modelName.toStdString());
        
        // Update status as availability might change
        UpdateStatusLabel();
    }
}

void AIAnalysisPanel::OnShowInstalledModels()
{
    if (!m_aiService)
    {
        QMessageBox::warning(qobject_cast<QWidget*>(parent()),
            tr("AI Service"),
            tr("AI service not initialized"));
        return;
    }
    
    auto* ollamaClient = dynamic_cast<ai::OllamaClient*>(m_aiService.get());
    if (!ollamaClient)
    {
        QMessageBox::warning(qobject_cast<QWidget*>(parent()),
            tr("AI Service"),
            tr("Not using Ollama client"));
        return;
    }
    
    const auto models = ollamaClient->GetInstalledModels();
    
    if (models.empty())
    {
        QMessageBox::information(qobject_cast<QWidget*>(parent()),
            tr("Installed Models"),
            tr("No models found.\n\n"
               "Download models using:\n"
               "  ollama pull qwen2.5-coder:7b\n"
               "  ollama pull llama3.2"));
        return;
    }
    
    QString modelList = tr("<h3>Installed Ollama Models</h3>");
    modelList += tr("<p>Found %1 model(s):</p><ul>").arg(models.size());
    
    for (const auto& model : models)
    {
        modelList += QString("<li><b>%1</b>").arg(QString::fromStdString(model.name));
        if (!model.size.empty())
            modelList += QString(" - %1").arg(QString::fromStdString(model.size));
        modelList += "</li>";
    }
    
    modelList += "</ul>";
    modelList += tr("<p><i>To use a model, select it from the Model dropdown above.</i></p>");
    
    QMessageBox msgBox(qobject_cast<QWidget*>(parent()));
    msgBox.setWindowTitle(tr("Installed Models"));
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(modelList);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
}

void AIAnalysisPanel::UpdateStatusLabel()
{
    if (!m_analyzer)
    {
        m_statusLabel->setText(tr("Not initialized"));
        m_statusLabel->setStyleSheet("QLabel { color: red; }");
        m_analyzeButton->setEnabled(false);
        return;
    }

    if (m_analyzer->IsReady())
    {
        m_statusLabel->setText(tr("Ready"));
        m_statusLabel->setStyleSheet("QLabel { color: green; }");
        m_analyzeButton->setEnabled(true);
    }
    else
    {
        m_statusLabel->setText(
            tr("<a href='#'>Ollama not available - Click here for setup</a>"));
        m_statusLabel->setStyleSheet("QLabel { color: orange; }");
        m_statusLabel->setTextFormat(Qt::RichText);
        m_statusLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
        m_statusLabel->setOpenExternalLinks(false);
        
        disconnect(m_statusLabel, &QLabel::linkActivated, nullptr, nullptr);
        connect(m_statusLabel, &QLabel::linkActivated, this, &AIAnalysisPanel::OnSetupClicked);
        
        m_analyzeButton->setEnabled(false);
    }
}

void AIAnalysisPanel::PopulateModelList()
{
    if (!m_modelCombo)
    {
        util::Logger::Error("PopulateModelList called before m_modelCombo initialized");
        return;
    }
    
    // Block signals to avoid triggering OnModelChanged during population
    m_modelCombo->blockSignals(true);
    m_modelCombo->clear();
    
    if (!m_aiService)
    {
        m_modelCombo->addItem(tr("No AI service available"), "");
        m_modelCombo->blockSignals(false);
        return;
    }
    
    auto* ollamaClient = dynamic_cast<ai::OllamaClient*>(m_aiService.get());
    if (!ollamaClient)
    {
        m_modelCombo->addItem(tr("Not using Ollama client"), "");
        m_modelCombo->blockSignals(false);
        return;
    }
    
    try
    {
        const auto models = ollamaClient->GetInstalledModels();
        
        if (models.empty())
        {
            m_modelCombo->addItem(tr("No models installed - run 'ollama pull <model>'"), "");
            util::Logger::Warn("No Ollama models found. User needs to install models.");
            m_modelCombo->blockSignals(false);
            return;
        }
        
        // Add installed models to dropdown
        for (const auto& model : models)
        {
            QString displayName = QString::fromStdString(model.name);
            if (!model.size.empty())
                displayName += QString(" (%1)").arg(QString::fromStdString(model.size));
                
            m_modelCombo->addItem(displayName, QString::fromStdString(model.name));
        }
        
        // Try to select the configured default model
        const auto& cfg = config::GetConfig();
        const int defaultIdx = m_modelCombo->findData(QString::fromStdString(cfg.ollamaDefaultModel));
        if (defaultIdx >= 0)
        {
            m_modelCombo->setCurrentIndex(defaultIdx);
        }
        
        util::Logger::Info("Populated model list with {} models", models.size());
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("Failed to populate model list: {}", e.what());
        m_modelCombo->addItem(tr("Error loading models: %1").arg(e.what()), "");
    }
    
    // Re-enable signals
    m_modelCombo->blockSignals(false);
    
    // Manually set the model to match current selection
    if (m_modelCombo->count() > 0 && m_aiService)
    {
        const QString selectedModel = m_modelCombo->currentData().toString();
        if (!selectedModel.isEmpty())
        {
            if (auto* client = dynamic_cast<ai::OllamaClient*>(m_aiService.get()))
            {
                client->SetModel(selectedModel.toStdString());
                util::Logger::Info("Initial model set to: {}", selectedModel.toStdString());
            }
        }
    }
}

void AIAnalysisPanel::OnRefreshModels()
{
    PopulateModelList();
    QMessageBox::information(this, tr("Models Refreshed"), 
        tr("Model list has been refreshed.\nFound %1 model(s).").arg(m_modelCombo->count()));
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
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Load Prompt from File"),
        QDir::currentPath() + "/etc/prompts",
        tr("Prompt Files (*.txt *.prompt);;All Files (*)")
    );
    
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
    
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Save Prompt to File"),
        QDir::currentPath() + "/etc/prompts/my_prompt.txt",
        tr("Text Files (*.txt);;Prompt Files (*.prompt);;All Files (*)")
    );
    
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
