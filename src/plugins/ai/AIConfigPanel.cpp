#include "plugins/ai/AIConfigPanel.hpp"
#include "plugins/ai/OllamaClient.hpp"
#include "plugins/ai/OpenAIClient.hpp"
#include "plugins/ai/AnthropicClient.hpp"
#include "plugins/ai/GeminiClient.hpp"
#include "plugins/ai/AIServiceFactory.hpp"
#include "plugins/PluginManager.hpp"
#include "config/Config.hpp"
#include "util/Logger.hpp"
#include "util/KeyEncryption.hpp"

#include <fstream>
#include <filesystem>

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
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    BuildUi();
    UpdateStatusLabel();
}

void AIConfigPanel::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

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
    
    // Load from plugin config (with main config as fallback)
    nlohmann::json pluginConfig = LoadPluginConfig();
    const auto& cfg = config::GetConfig();
    std::string provider = pluginConfig.value("provider", cfg.aiProvider);
    const int providerIdx = m_providerCombo->findData(QString::fromStdString(provider));
    if (providerIdx >= 0)
        m_providerCombo->setCurrentIndex(providerIdx);
    
    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AIConfigPanel::OnProviderChanged);
    configLayout->addRow(tr("Provider:"), m_providerCombo);

    // Base URL
    m_baseUrlEdit = new QLineEdit(this);
    m_baseUrlEdit->setPlaceholderText("http://localhost:11434");
    std::string baseUrl = pluginConfig.value("baseUrl", cfg.ollamaBaseUrl);
    m_baseUrlEdit->setText(QString::fromStdString(baseUrl));
    connect(m_baseUrlEdit, &QLineEdit::editingFinished, [this]() {
        SavePluginConfig();
        RefreshAIClient();
        emit ConfigurationChanged();
    });
    configLayout->addRow(tr("Base URL:"), m_baseUrlEdit);

    // API Keys group for cloud providers
    auto* apiKeysGroup = new QGroupBox(tr("Cloud Provider API Keys"), this);
    apiKeysGroup->setToolTip(tr("API keys are stored encrypted. Only required for cloud providers."));
    auto* apiKeysLayout = new QFormLayout(apiKeysGroup);
    
    m_openaiKeyEdit = new QLineEdit(apiKeysGroup);
    m_openaiKeyEdit->setEchoMode(QLineEdit::Password);
    m_openaiKeyEdit->setPlaceholderText(tr("OpenAI API key (from platform.openai.com)"));
    std::string openaiKey = pluginConfig.value("openaiApiKey", cfg.openaiApiKey);
    m_openaiKeyEdit->setText(QString::fromStdString(util::KeyEncryption::Decrypt(openaiKey)));
    apiKeysLayout->addRow(tr("OpenAI:"), m_openaiKeyEdit);
    
    m_anthropicKeyEdit = new QLineEdit(apiKeysGroup);
    m_anthropicKeyEdit->setEchoMode(QLineEdit::Password);
    m_anthropicKeyEdit->setPlaceholderText(tr("Anthropic API key (from console.anthropic.com)"));
    std::string anthropicKey = pluginConfig.value("anthropicApiKey", cfg.anthropicApiKey);
    m_anthropicKeyEdit->setText(QString::fromStdString(util::KeyEncryption::Decrypt(anthropicKey)));
    apiKeysLayout->addRow(tr("Anthropic:"), m_anthropicKeyEdit);
    
    m_googleKeyEdit = new QLineEdit(apiKeysGroup);
    m_googleKeyEdit->setEchoMode(QLineEdit::Password);
    m_googleKeyEdit->setPlaceholderText(tr("Google API key (from makersuite.google.com)"));
    std::string googleKey = pluginConfig.value("googleApiKey", cfg.googleApiKey);
    m_googleKeyEdit->setText(QString::fromStdString(util::KeyEncryption::Decrypt(googleKey)));
    apiKeysLayout->addRow(tr("Google:"), m_googleKeyEdit);
    
    m_xaiKeyEdit = new QLineEdit(apiKeysGroup);
    m_xaiKeyEdit->setEchoMode(QLineEdit::Password);
    m_xaiKeyEdit->setPlaceholderText(tr("xAI API key (from console.x.ai)"));
    std::string xaiKey = pluginConfig.value("xaiApiKey", cfg.xaiApiKey);
    m_xaiKeyEdit->setText(QString::fromStdString(util::KeyEncryption::Decrypt(xaiKey)));
    apiKeysLayout->addRow(tr("xAI:"), m_xaiKeyEdit);
    
    mainLayout->addWidget(apiKeysGroup);

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
    
    // Initialize AI client with current configuration
    RefreshAIClient();
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
    
    // Get default base URL for the new provider
    std::string newBaseUrl = ai::AIServiceFactory::GetDefaultBaseUrl(provider.toStdString());
    
    // Update UI field
    if (m_baseUrlEdit)
    {
        util::Logger::Debug("Updating base URL field to: {}", newBaseUrl);
        m_baseUrlEdit->setText(QString::fromStdString(newBaseUrl));
        util::Logger::Debug("Base URL field updated, current text: {}", m_baseUrlEdit->text().toStdString());
    }
    
    // Load current config, update it, and save
    nlohmann::json pluginConfig = LoadPluginConfig();
    pluginConfig["provider"] = provider.toStdString();
    pluginConfig["baseUrl"] = newBaseUrl;
    pluginConfig["model"] = "";  // Clear saved model so new provider starts fresh
    SavePluginConfig();
    
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
    
    // Update plugin config
    nlohmann::json pluginConfig = LoadPluginConfig();
    pluginConfig["model"] = modelName.toStdString();
    SavePluginConfig();

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
    
    // Get current provider from the service (not from main config)
    std::string currentProvider = m_aiService->service->GetProviderName();
    
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
    else if (currentProvider == "openai")
    {
        m_modelCombo->addItem("GPT-3.5 Turbo (Recommended)", "gpt-3.5-turbo");
        m_modelCombo->addItem("GPT-4o Mini", "gpt-4o-mini");
        m_modelCombo->addItem("GPT-4o", "gpt-4o");
        m_modelCombo->addItem("GPT-4 Turbo", "gpt-4-turbo");
        m_modelCombo->addItem("GPT-4", "gpt-4");
    }
    else if (currentProvider == "anthropic")
    {
        m_modelCombo->addItem("Claude 3.5 Sonnet", "claude-3-5-sonnet-20241022");
        m_modelCombo->addItem("Claude 3 Opus", "claude-3-opus-20240229");
        m_modelCombo->addItem("Claude 3 Sonnet", "claude-3-sonnet-20240229");
        m_modelCombo->addItem("Claude 3 Haiku", "claude-3-haiku-20240307");
    }
    else if (currentProvider == "google")
    {
        m_modelCombo->addItem("Gemini 2.5 Pro", "gemini-2.5-pro");
        m_modelCombo->addItem("Gemini 2.5 Lite", "gemini-2.5-flash-lite");
        m_modelCombo->addItem("Gemini 2.5 Flash", "gemini-2.5-flash");
        m_modelCombo->addItem("Gemini 3 Pro", "gemini-3-pro");
    }
    else if (currentProvider == "xai")
    {
        m_modelCombo->addItem("Grok Beta", "grok-beta");
        m_modelCombo->addItem("Grok 2", "grok-2");
    }
    
    m_modelCombo->blockSignals(false);
    
    // Load plugin config to get saved model
    nlohmann::json pluginConfig = LoadPluginConfig();
    const auto& mainConfig = config::GetConfig();
    std::string savedModel = pluginConfig.value("model", mainConfig.ollamaDefaultModel);
    
    // Select the configured model if it exists, otherwise select the first one
    bool modelFound = false;
    for (int i = 0; i < m_modelCombo->count(); ++i)
    {
        if (m_modelCombo->itemData(i).toString().toStdString() == savedModel)
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
    nlohmann::json pluginConfig = LoadPluginConfig();
    auto& mainConfig = config::GetConfig();
    
    // Plugin config takes priority, main config as fallback
    std::string provider = pluginConfig.value("provider", mainConfig.aiProvider);
    std::string baseUrl = pluginConfig.value("baseUrl", mainConfig.ollamaBaseUrl);
    std::string model = pluginConfig.value("model", mainConfig.ollamaDefaultModel);
    
    // Get API key for the selected provider
    std::string apiKey;
    if (provider == "openai")
    {
        std::string encryptedKey = pluginConfig.value("openaiApiKey", mainConfig.openaiApiKey);
        util::Logger::Debug("OpenAI encrypted key from config: {} chars", encryptedKey.size());
        apiKey = util::KeyEncryption::Decrypt(encryptedKey);
        util::Logger::Debug("OpenAI decrypted key: {} chars", apiKey.size());
    }
    else if (provider == "anthropic")
    {
        std::string encryptedKey = pluginConfig.value("anthropicApiKey", mainConfig.anthropicApiKey);
        util::Logger::Debug("Anthropic encrypted key from config: {} chars", encryptedKey.size());
        apiKey = util::KeyEncryption::Decrypt(encryptedKey);
        util::Logger::Debug("Anthropic decrypted key: {} chars", apiKey.size());
    }
    else if (provider == "google")
    {
        std::string encryptedKey = pluginConfig.value("googleApiKey", mainConfig.googleApiKey);
        util::Logger::Debug("Google encrypted key from config: {} chars", encryptedKey.size());
        apiKey = util::KeyEncryption::Decrypt(encryptedKey);
        util::Logger::Debug("Google decrypted key: {} chars", apiKey.size());
    }
    else if (provider == "xai")
    {
        std::string encryptedKey = pluginConfig.value("xaiApiKey", mainConfig.xaiApiKey);
        util::Logger::Debug("xAI encrypted key from config: {} chars", encryptedKey.size());
        apiKey = util::KeyEncryption::Decrypt(encryptedKey);
        util::Logger::Debug("xAI decrypted key: {} chars", apiKey.size());
    }
    
    try
    {
        m_aiService->service = ai::AIServiceFactory::CreateClient(
            provider,
            apiKey,
            baseUrl,
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

nlohmann::json AIConfigPanel::LoadPluginConfig()
{
    auto& pluginManager = plugin::PluginManager::GetInstance();
    std::filesystem::path configPath = pluginManager.GetPluginConfigPath("ai_provider");
    
    if (!std::filesystem::exists(configPath))
    {
        util::Logger::Debug("[AIConfigPanel] No plugin config file found, using defaults");
        return nlohmann::json::object();
    }

    try
    {
        std::ifstream file(configPath);
        if (!file.is_open())
        {
            util::Logger::Warn("[AIConfigPanel] Failed to open plugin config file");
            return nlohmann::json::object();
        }

        nlohmann::json config;
        file >> config;
        return config;
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[AIConfigPanel] Failed to parse plugin config: {}", ex.what());
        return nlohmann::json::object();
    }
}

void AIConfigPanel::SavePluginConfig()
{
    auto& pluginManager = plugin::PluginManager::GetInstance();
    std::filesystem::path configPath = pluginManager.GetPluginConfigPath("ai_provider");
    
    try
    {
        // Load existing config
        nlohmann::json config = LoadPluginConfig();
        
        // Update with current values
        if (m_providerCombo)
        {
            QString provider = m_providerCombo->itemData(m_providerCombo->currentIndex()).toString();
            if (!provider.isEmpty())
            {
                config["provider"] = provider.toStdString();
            }
        }
        
        if (m_baseUrlEdit)
        {
            config["baseUrl"] = m_baseUrlEdit->text().toStdString();
        }
        
        if (m_modelCombo && m_modelCombo->count() > 0)
        {
            QString model = m_modelCombo->itemData(m_modelCombo->currentIndex()).toString();
            if (!model.isEmpty())
            {
                config["model"] = model.toStdString();
            }
        }
        
        // Save API keys (encrypt them)
        if (m_openaiKeyEdit && !m_openaiKeyEdit->text().isEmpty())
        {
            std::string key = m_openaiKeyEdit->text().toStdString();
            if (!util::KeyEncryption::IsEncrypted(key))
            {
                key = util::KeyEncryption::Encrypt(key);
            }
            config["openaiApiKey"] = key;
        }
        if (m_anthropicKeyEdit && !m_anthropicKeyEdit->text().isEmpty())
        {
            std::string key = m_anthropicKeyEdit->text().toStdString();
            if (!util::KeyEncryption::IsEncrypted(key))
            {
                key = util::KeyEncryption::Encrypt(key);
            }
            config["anthropicApiKey"] = key;
        }
        if (m_googleKeyEdit && !m_googleKeyEdit->text().isEmpty())
        {
            std::string key = m_googleKeyEdit->text().toStdString();
            if (!util::KeyEncryption::IsEncrypted(key))
            {
                key = util::KeyEncryption::Encrypt(key);
            }
            config["googleApiKey"] = key;
        }
        if (m_xaiKeyEdit && !m_xaiKeyEdit->text().isEmpty())
        {
            std::string key = m_xaiKeyEdit->text().toStdString();
            if (!util::KeyEncryption::IsEncrypted(key))
            {
                key = util::KeyEncryption::Encrypt(key);
            }
            config["xaiApiKey"] = key;
        }
        
        // Ensure directory exists
        std::filesystem::path configDir = configPath.parent_path();
        if (!std::filesystem::exists(configDir))
        {
            std::filesystem::create_directories(configDir);
        }

        std::ofstream file(configPath);
        if (!file.is_open())
        {
            util::Logger::Error("[AIConfigPanel] Failed to open config file for writing");
            return;
        }

        file << config.dump(2);
        file.close();
        util::Logger::Info("[AIConfigPanel] Saved plugin config to: {}", configPath.string());
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[AIConfigPanel] Failed to save plugin config: {}", ex.what());
    }
}

} // namespace ui::qt
