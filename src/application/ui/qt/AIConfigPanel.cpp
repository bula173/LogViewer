#include "ui/qt/AIConfigPanel.hpp"
#include "ai/OllamaClient.hpp"
#include "ai/OpenAIClient.hpp"
#include "ai/AnthropicClient.hpp"
#include "ai/GeminiClient.hpp"
#include "ai/AIServiceFactory.hpp"
#include "config/Config.hpp"
#include "util/Logger.hpp"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QMessageBox>

namespace ui::qt
{

AIConfigPanel::AIConfigPanel(std::shared_ptr<ai::AIServiceWrapper> aiService,
                             std::shared_ptr<ai::LogAnalyzer> analyzer,
                             QWidget* parent)
    : QWidget(parent)
    , m_aiService(aiService)
    , m_analyzer(analyzer)
{
    BuildUi();
    UpdateStatusLabel();
}

void AIConfigPanel::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // AI Configuration group
    auto* configGroup = new QGroupBox(tr("AI Configuration"), this);
    auto* configLayout = new QFormLayout(configGroup);
    configLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // Provider selector
    m_providerCombo = new QComboBox(this);
    m_providerCombo->setToolTip(tr("Select AI provider"));
    m_providerCombo->addItem("Ollama (Local)", "ollama");
    m_providerCombo->addItem("LM Studio (Local)", "lmstudio");
    m_providerCombo->addItem("OpenAI (Cloud)", "openai");
    m_providerCombo->addItem("Anthropic Claude (Cloud)", "anthropic");
    m_providerCombo->addItem("Google Gemini (Cloud)", "google");
    m_providerCombo->addItem("xAI Grok (Cloud)", "xai");
    
    const auto& cfg = config::GetConfig();
    const int providerIdx = m_providerCombo->findData(QString::fromStdString(cfg.aiProvider));
    if (providerIdx >= 0)
        m_providerCombo->setCurrentIndex(providerIdx);
    
    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AIConfigPanel::OnProviderChanged);
    configLayout->addRow(tr("Provider:"), m_providerCombo);

    // Model selector with buttons
    m_modelCombo = new QComboBox(this);
    m_modelCombo->setToolTip(tr("Select AI model"));
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AIConfigPanel::OnModelChanged);
    
    auto* modelButtonsLayout = new QVBoxLayout();
    modelButtonsLayout->addWidget(m_modelCombo);
    
    auto* modelBtnRow = new QHBoxLayout();
    auto* refreshBtn = new QPushButton(tr("Refresh"), this);
    refreshBtn->setToolTip(tr("Refresh model list"));
    connect(refreshBtn, &QPushButton::clicked, this, &AIConfigPanel::OnRefreshModels);
    modelBtnRow->addWidget(refreshBtn);
    
    auto* detailsBtn = new QPushButton(tr("Details"), this);
    detailsBtn->setToolTip(tr("Show model details"));
    connect(detailsBtn, &QPushButton::clicked, this, &AIConfigPanel::OnShowInstalledModels);
    modelBtnRow->addWidget(detailsBtn);
    modelButtonsLayout->addLayout(modelBtnRow);
    
    configLayout->addRow(tr("Model:"), modelButtonsLayout);
    
    PopulateModelList();

    // Analysis type
    m_analysisTypeCombo = new QComboBox(this);
    m_analysisTypeCombo->setToolTip(tr("Type of analysis to perform"));
    m_analysisTypeCombo->addItem(tr("Summary"), 
        static_cast<int>(ai::LogAnalyzer::AnalysisType::Summary));
    m_analysisTypeCombo->addItem(tr("Error Analysis"), 
        static_cast<int>(ai::LogAnalyzer::AnalysisType::ErrorAnalysis));
    m_analysisTypeCombo->addItem(tr("Pattern Detection"), 
        static_cast<int>(ai::LogAnalyzer::AnalysisType::PatternDetection));
    m_analysisTypeCombo->addItem(tr("Root Cause"), 
        static_cast<int>(ai::LogAnalyzer::AnalysisType::RootCause));
    m_analysisTypeCombo->addItem(tr("Timeline"), 
        static_cast<int>(ai::LogAnalyzer::AnalysisType::Timeline));
    configLayout->addRow(tr("Analysis Type:"), m_analysisTypeCombo);

    // Max events
    m_maxEventsSpin = new QSpinBox(this);
    m_maxEventsSpin->setMinimum(10);
    m_maxEventsSpin->setMaximum(1000);
    m_maxEventsSpin->setValue(100);
    m_maxEventsSpin->setSingleStep(10);
    m_maxEventsSpin->setToolTip(tr("Maximum events to analyze"));
    configLayout->addRow(tr("Max Events:"), m_maxEventsSpin);

    // Status
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextFormat(Qt::RichText);
    configLayout->addRow(tr("Status:"), m_statusLabel);

    mainLayout->addWidget(configGroup);
    mainLayout->addStretch();
}

ai::LogAnalyzer::AnalysisType AIConfigPanel::GetAnalysisType() const
{
    if (!m_analysisTypeCombo)
        return ai::LogAnalyzer::AnalysisType::Summary;
    
    return static_cast<ai::LogAnalyzer::AnalysisType>(
        m_analysisTypeCombo->currentData().toInt());
}

int AIConfigPanel::GetMaxEvents() const
{
    return m_maxEventsSpin ? m_maxEventsSpin->value() : 100;
}

void AIConfigPanel::OnProviderChanged(int index)
{
    if (index < 0)
        return;
    
    const QString provider = m_providerCombo->itemData(index).toString();
    if (provider.isEmpty())
        return;
    
    util::Logger::Info("AI provider changed to: {}", provider.toStdString());
    
    auto& config = config::GetConfig();
    config.aiProvider = provider.toStdString();
    config.ollamaBaseUrl = QString::fromStdString(
        ai::AIServiceFactory::GetDefaultBaseUrl(config.aiProvider)).toStdString();
    
    // Clear saved model so new provider starts fresh
    config.ollamaDefaultModel = "";
    
    RefreshAIClient();
    emit ConfigurationChanged();
}

void AIConfigPanel::OnModelChanged(int index)
{
    if (!m_aiService || !m_aiService.get() || index < 0)
        return;
    
    const QString modelName = m_modelCombo->itemData(index).toString();
    if (modelName.isEmpty())
        return;
    
    util::Logger::Info("AI model changed to: {}", modelName.toStdString());
    
    // Update config
    auto& config = config::GetConfig();
    config.ollamaDefaultModel = modelName.toStdString();

    m_aiService->service->SetModelName(modelName.toStdString());
    
    UpdateStatusLabel();
    emit ConfigurationChanged();
}

void AIConfigPanel::OnRefreshModels()
{
    PopulateModelList();
}

void AIConfigPanel::OnShowInstalledModels()
{
    if (!m_aiService || !m_aiService.get())
    {
        QMessageBox::information(this, tr("AI Models"), 
            tr("AI service not initialized"));
        return;
    }
    
    auto* ollamaClient = dynamic_cast<ai::OllamaClient*>(m_aiService->service.get());
    if (!ollamaClient)
    {
        QMessageBox::information(this, tr("AI Models"),
            tr("Model details only available for Ollama"));
        return;
    }
    
    const auto models = ollamaClient->GetInstalledModels();
    if (models.empty())
    {
        QMessageBox::information(this, tr("Installed Models"),
            tr("No models found.\n\nDownload with:\n  ollama pull qwen2.5-coder:7b"));
        return;
    }
    
    QString modelList = tr("<h3>Installed Models</h3><ul>");
    for (const auto& model : models)
    {
        modelList += QString("<li><b>%1</b>").arg(QString::fromStdString(model.name));
        if (!model.size.empty())
            modelList += QString(" - %1").arg(QString::fromStdString(model.size));
        modelList += "</li>";
    }
    modelList += "</ul>";
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Installed Models"));
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(modelList);
    msgBox.exec();
}

void AIConfigPanel::UpdateStatusLabel()
{
    if (!m_statusLabel)
        return;
        
    if (!m_analyzer || !m_analyzer.get())
    {
        m_statusLabel->setText(tr("Not initialized"));
        m_statusLabel->setStyleSheet("QLabel { color: gray; }");
        return;
    }

    if (m_analyzer->IsReady())
    {
        m_statusLabel->setText(tr("Ready"));
        m_statusLabel->setStyleSheet("QLabel { color: green; }");
    }
    else
    {
        m_statusLabel->setText(tr("Not available"));
        m_statusLabel->setStyleSheet("QLabel { color: orange; }");
    }
}

void AIConfigPanel::PopulateModelList()
{
    if (!m_modelCombo)
        return;
    
    QString currentModel;
    if (m_modelCombo->currentIndex() >= 0)
        currentModel = m_modelCombo->currentData().toString();
    
    m_modelCombo->blockSignals(true);
    m_modelCombo->clear();
    
    if (!m_aiService || !m_aiService.get())
    {
        m_modelCombo->addItem(tr("No AI service"), "");
        m_modelCombo->blockSignals(false);
        return;
    }
    
    const auto& cfg = config::GetConfig();
    
    auto* ollamaClient = dynamic_cast<ai::OllamaClient*>(m_aiService->service.get());
    if (ollamaClient)
    {
        try
        {
            const auto models = ollamaClient->GetInstalledModels();
            if (models.empty())
            {
                m_modelCombo->addItem(tr("No models installed"), "");
            }
            else
            {
                for (const auto& model : models)
                {
                    QString displayName = QString::fromStdString(model.name);
                    if (!model.size.empty())
                        displayName += QString(" (%1)").arg(QString::fromStdString(model.size));
                    m_modelCombo->addItem(displayName, QString::fromStdString(model.name));
                }
            }
        }
        catch (const std::exception& e)
        {
            m_modelCombo->addItem(tr("Error: %1").arg(e.what()), "");
        }
    }
    else if (cfg.aiProvider == "openai")
    {
        m_modelCombo->addItem("GPT-3.5 Turbo (Recommended)", "gpt-3.5-turbo");
        m_modelCombo->addItem("GPT-4o Mini", "gpt-4o-mini");
        m_modelCombo->addItem("GPT-4o", "gpt-4o");
        m_modelCombo->addItem("GPT-4 Turbo", "gpt-4-turbo");
        m_modelCombo->addItem("GPT-4", "gpt-4");
    }
    else if (cfg.aiProvider == "anthropic")
    {
        m_modelCombo->addItem("Claude 3.5 Sonnet", "claude-3-5-sonnet-20241022");
        m_modelCombo->addItem("Claude 3 Opus", "claude-3-opus-20240229");
        m_modelCombo->addItem("Claude 3 Sonnet", "claude-3-sonnet-20240229");
        m_modelCombo->addItem("Claude 3 Haiku", "claude-3-haiku-20240307");
    }
    else if (cfg.aiProvider == "google")
    {
        m_modelCombo->addItem("Gemini 2.5 Pro", "gemini-2.5-pro");
        m_modelCombo->addItem("Gemini 2.5 Lite", "gemini-2.5-flash-lite");
        m_modelCombo->addItem("Gemini 2.5 Flash", "gemini-2.5-flash");
        m_modelCombo->addItem("Gemini 3 Pro", "gemini-3-pro");
    }
    else if (cfg.aiProvider == "xai")
    {
        m_modelCombo->addItem("Grok Beta", "grok-beta");
        m_modelCombo->addItem("Grok 2", "grok-2");
    }
    
    m_modelCombo->blockSignals(false);
    
    // Select the configured model if it exists, otherwise select the first one
    bool modelFound = false;
    for (int i = 0; i < m_modelCombo->count(); ++i)
    {
        if (m_modelCombo->itemData(i).toString().toStdString() == cfg.ollamaDefaultModel)
        {
            m_modelCombo->setCurrentIndex(i);
            modelFound = true;
            break;
        }
    }
    
    if (!modelFound && m_modelCombo->count() > 0)
    {
        m_modelCombo->setCurrentIndex(0);
    }
    
    // Apply the selected model
    if (m_modelCombo->count() > 0)
    {
        OnModelChanged(m_modelCombo->currentIndex());
    }
}

void AIConfigPanel::RefreshAIClient()
{
    auto& config = config::GetConfig();
    try
    {
        m_aiService->service = ai::AIServiceFactory::CreateClient(
            config.aiProvider,
            config.GetApiKeyForProvider(config.aiProvider),
            config.ollamaBaseUrl,
            ""  // Empty model - will be set by PopulateModelList
        );        
        // PopulateModelList will automatically select and apply the first available model
        PopulateModelList();
        
        UpdateStatusLabel();
        util::Logger::Info("AI client refreshed: provider={}", m_aiService->service->GetProviderName());
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("Failed to refresh AI client: {}", e.what());
        if (m_statusLabel)
        {
            m_statusLabel->setText(tr("Error: %1").arg(e.what()));
            m_statusLabel->setStyleSheet("QLabel { color: red; }");
        }
    }
}

} // namespace ui::qt
