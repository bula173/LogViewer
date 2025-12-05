#include "ui/qt/AIAnalysisPanel.hpp"
#include "ui/qt/OllamaSetupDialog.hpp"
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

namespace ui::qt
{

AIAnalysisPanel::AIAnalysisPanel(std::shared_ptr<ai::IAIService> aiService,
                                 std::shared_ptr<ai::LogAnalyzer> analyzer,
                                 QWidget* parent)
    : QWidget(parent)
    , m_aiService(std::move(aiService))
    , m_analyzer(std::move(analyzer))
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

    // Get selected analysis type
    const int typeIndex = m_analysisTypeCombo->currentData().toInt();
    const auto analysisType = static_cast<ai::LogAnalyzer::AnalysisType>(typeIndex);
    const size_t maxEvents = static_cast<size_t>(m_maxEventsSpin->value());

    util::Logger::Info("Starting AI analysis: type={}, maxEvents={}",
        ai::LogAnalyzer::GetAnalysisTypeName(analysisType), maxEvents);

    try
    {
        const std::string result = m_analyzer->Analyze(analysisType, maxEvents);
        m_resultsText->setPlainText(QString::fromStdString(result));
        m_statusLabel->setText(tr("Analysis complete"));
        
        emit AnalysisCompleted(QString::fromStdString(result));
    }
    catch (const std::exception& e)
    {
        const QString error = tr("Analysis failed: %1").arg(e.what());
        m_resultsText->setPlainText(error);
        m_statusLabel->setText(tr("Error"));
        util::Logger::Error("AI analysis failed: {}", e.what());
    }

    m_analyzeButton->setEnabled(true);
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
            if (auto* ollamaClient = dynamic_cast<ai::OllamaClient*>(m_aiService.get()))
            {
                ollamaClient->SetModel(selectedModel.toStdString());
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

} // namespace ui::qt
