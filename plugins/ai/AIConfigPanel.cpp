#include "AIConfigPanel.hpp"
#include "OllamaClient.hpp"
#include "OpenAIClient.hpp"
#include "AnthropicClient.hpp"
#include "GeminiClient.hpp"
#include "AIServiceFactory.hpp"
#include "Config.hpp"
#include "PluginLoggerC.h"
#include "PluginKeyEncryptionC.h"
#include <fmt/format.h>

// Helper macro for formatted plugin logging
#define PLUGIN_LOG(level, ...) do { std::string _pl_msg = fmt::format(__VA_ARGS__); PluginLogger_Log(level, _pl_msg.c_str()); } while(0)

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
#include <QCoreApplication>

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

    // Timeout for AI requests
    m_timeoutSpin = new QSpinBox(this);
    m_timeoutSpin->setMinimum(1);
    m_timeoutSpin->setMaximum(600);
    int timeoutVal = pluginConfig.value("aiTimeoutSeconds", cfg.aiTimeoutSeconds);
    m_timeoutSpin->setValue(timeoutVal);
    m_timeoutSpin->setToolTip(tr("Timeout for AI requests in seconds"));
    connect(m_timeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int){
        SavePluginConfig();
        RefreshAIClient();
        emit ConfigurationChanged();
    });
    configLayout->addRow(tr("Timeout (s):"), m_timeoutSpin);

    // API Keys group for cloud providers
    auto* apiKeysGroup = new QGroupBox(tr("Cloud Provider API Keys"), this);
    apiKeysGroup->setToolTip(tr("API keys are stored encrypted. Only required for cloud providers."));
    auto* apiKeysLayout = new QFormLayout(apiKeysGroup);
    
    m_openaiKeyEdit = new QLineEdit(apiKeysGroup);
    m_openaiKeyEdit->setEchoMode(QLineEdit::Password);
    m_openaiKeyEdit->setPlaceholderText(tr("OpenAI API key (from platform.openai.com)"));
    std::string openaiKey = pluginConfig.value("openaiApiKey", cfg.openaiApiKey);
    if (!openaiKey.empty()) {
        char* dec = PluginKeyEncryption_Decrypt(openaiKey.c_str());
        if (dec) {
            m_openaiKeyEdit->setText(QString::fromStdString(dec));
            PluginKeyEncryption_FreeString(dec);
        }
    }
    apiKeysLayout->addRow(tr("OpenAI:"), m_openaiKeyEdit);
    
    m_anthropicKeyEdit = new QLineEdit(apiKeysGroup);
    m_anthropicKeyEdit->setEchoMode(QLineEdit::Password);
    m_anthropicKeyEdit->setPlaceholderText(tr("Anthropic API key (from console.anthropic.com)"));
    std::string anthropicKey = pluginConfig.value("anthropicApiKey", cfg.anthropicApiKey);
    if (!anthropicKey.empty()) {
        char* dec = PluginKeyEncryption_Decrypt(anthropicKey.c_str());
        if (dec) {
            m_anthropicKeyEdit->setText(QString::fromStdString(dec));
            PluginKeyEncryption_FreeString(dec);
        }
    }
    apiKeysLayout->addRow(tr("Anthropic:"), m_anthropicKeyEdit);
    
    m_googleKeyEdit = new QLineEdit(apiKeysGroup);
    m_googleKeyEdit->setEchoMode(QLineEdit::Password);
    m_googleKeyEdit->setPlaceholderText(tr("Google API key (from makersuite.google.com)"));
    std::string googleKey = pluginConfig.value("googleApiKey", cfg.googleApiKey);
    if (!googleKey.empty()) {
        char* dec = PluginKeyEncryption_Decrypt(googleKey.c_str());
        if (dec) {
            m_googleKeyEdit->setText(QString::fromStdString(dec));
            PluginKeyEncryption_FreeString(dec);
        }
    }
    apiKeysLayout->addRow(tr("Google:"), m_googleKeyEdit);
    
    m_xaiKeyEdit = new QLineEdit(apiKeysGroup);
    m_xaiKeyEdit->setEchoMode(QLineEdit::Password);
    m_xaiKeyEdit->setPlaceholderText(tr("xAI API key (from console.x.ai)"));
    std::string xaiKey = pluginConfig.value("xaiApiKey", cfg.xaiApiKey);
    if (!xaiKey.empty()) {
        char* dec = PluginKeyEncryption_Decrypt(xaiKey.c_str());
        if (dec) {
            m_xaiKeyEdit->setText(QString::fromStdString(dec));
            PluginKeyEncryption_FreeString(dec);
        }
    }
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
    
    PLUGIN_LOG(PLUGIN_LOG_INFO, "AI provider changed to: {}", provider.toStdString());
    
    // Get default base URL for the new provider
    std::string newBaseUrl = ai::AIServiceFactory::GetDefaultBaseUrl(provider.toStdString());
    
    // Update UI field
    if (m_baseUrlEdit)
    {
        PLUGIN_LOG(PLUGIN_LOG_DEBUG, "Updating base URL field to: {}", newBaseUrl);
        m_baseUrlEdit->setText(QString::fromStdString(newBaseUrl));
        PLUGIN_LOG(PLUGIN_LOG_DEBUG, "Base URL field updated, current text: {}", m_baseUrlEdit->text().toStdString());
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
    
    PLUGIN_LOG(PLUGIN_LOG_INFO, "AI model changed to: {}", modelName.toStdString());
    
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
        PLUGIN_LOG(PLUGIN_LOG_DEBUG, "OpenAI encrypted key from config: {} chars", encryptedKey.size());
        if (!encryptedKey.empty()) {
            char* dec = PluginKeyEncryption_Decrypt(encryptedKey.c_str());
            if (dec) {
                apiKey = std::string(dec);
                PluginKeyEncryption_FreeString(dec);
                PLUGIN_LOG(PLUGIN_LOG_DEBUG, "OpenAI decrypted key: {} chars", apiKey.size());
            }
        }
    }
    else if (provider == "anthropic")
    {
        std::string encryptedKey = pluginConfig.value("anthropicApiKey", mainConfig.anthropicApiKey);
        PLUGIN_LOG(PLUGIN_LOG_DEBUG, "Anthropic encrypted key from config: {} chars", encryptedKey.size());
        if (!encryptedKey.empty()) {
            char* dec = PluginKeyEncryption_Decrypt(encryptedKey.c_str());
            if (dec) {
                apiKey = std::string(dec);
                PluginKeyEncryption_FreeString(dec);
                PLUGIN_LOG(PLUGIN_LOG_DEBUG, "Anthropic decrypted key: {} chars", apiKey.size());
            }
        }
    }
    else if (provider == "google")
    {
        std::string encryptedKey = pluginConfig.value("googleApiKey", mainConfig.googleApiKey);
        PLUGIN_LOG(PLUGIN_LOG_DEBUG, "Google encrypted key from config: {} chars", encryptedKey.size());
        if (!encryptedKey.empty()) {
            char* dec = PluginKeyEncryption_Decrypt(encryptedKey.c_str());
            if (dec) {
                apiKey = std::string(dec);
                PluginKeyEncryption_FreeString(dec);
                PLUGIN_LOG(PLUGIN_LOG_DEBUG, "Google decrypted key: {} chars", apiKey.size());
            }
        }
    }
    else if (provider == "xai")
    {
        std::string encryptedKey = pluginConfig.value("xaiApiKey", mainConfig.xaiApiKey);
        PLUGIN_LOG(PLUGIN_LOG_DEBUG, "xAI encrypted key from config: {} chars", encryptedKey.size());
        if (!encryptedKey.empty()) {
            char* dec = PluginKeyEncryption_Decrypt(encryptedKey.c_str());
            if (dec) {
                apiKey = std::string(dec);
                PluginKeyEncryption_FreeString(dec);
                PLUGIN_LOG(PLUGIN_LOG_DEBUG, "xAI decrypted key: {} chars", apiKey.size());
            }
        }
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
        PLUGIN_LOG(PLUGIN_LOG_INFO, "AI client refreshed: provider={}", m_aiService->service->GetProviderName());
    }
    catch (const std::exception& e)
    {
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "Failed to refresh AI client: {}", e.what());
        if (m_statusLabel)
        {
            m_statusLabel->setText(tr("Error: %1").arg(e.what()));
            m_statusLabel->setStyleSheet("QLabel { color: red; }");
        }
    }
}

nlohmann::json AIConfigPanel::LoadPluginConfig()
{
    const std::string appDir = QCoreApplication::applicationDirPath().toStdString();
    std::filesystem::path configPath = std::filesystem::path(appDir) / "plugins" / "ai_provider" / "config.json";
    
    if (!std::filesystem::exists(configPath))
    {
        PLUGIN_LOG(PLUGIN_LOG_DEBUG, "[AIConfigPanel] No plugin config file found, using defaults");
        return nlohmann::json::object();
    }

    try
    {
        std::ifstream file(configPath);
        if (!file.is_open())
        {
            PLUGIN_LOG(PLUGIN_LOG_WARN, "[AIConfigPanel] Failed to open plugin config file");
            return nlohmann::json::object();
        }

        nlohmann::json config;
        file >> config;
        return config;
    }
    catch (const std::exception& ex)
    {
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "[AIConfigPanel] Failed to parse plugin config: {}", ex.what());
        return nlohmann::json::object();
    }
}

void AIConfigPanel::SavePluginConfig()
{
    const std::string appDir = QCoreApplication::applicationDirPath().toStdString();
    std::filesystem::path configPath = std::filesystem::path(appDir) / "plugins" / "ai_provider" / "config.json";
    
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

        if (m_timeoutSpin)
        {
            config["aiTimeoutSeconds"] = m_timeoutSpin->value();
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
            char* enc = PluginKeyEncryption_Encrypt(key.c_str());
            if (enc) { config["openaiApiKey"] = std::string(enc); PluginKeyEncryption_FreeString(enc); }
            else { config["openaiApiKey"] = key; }
        }
        if (m_anthropicKeyEdit && !m_anthropicKeyEdit->text().isEmpty())
        {
            std::string key = m_anthropicKeyEdit->text().toStdString();
            char* enc = PluginKeyEncryption_Encrypt(key.c_str());
            if (enc) { config["anthropicApiKey"] = std::string(enc); PluginKeyEncryption_FreeString(enc); }
            else { config["anthropicApiKey"] = key; }
        }
        if (m_googleKeyEdit && !m_googleKeyEdit->text().isEmpty())
        {
            std::string key = m_googleKeyEdit->text().toStdString();
            char* enc = PluginKeyEncryption_Encrypt(key.c_str());
            if (enc) { config["googleApiKey"] = std::string(enc); PluginKeyEncryption_FreeString(enc); }
            else { config["googleApiKey"] = key; }
        }
        if (m_xaiKeyEdit && !m_xaiKeyEdit->text().isEmpty())
        {
            std::string key = m_xaiKeyEdit->text().toStdString();
            char* enc = PluginKeyEncryption_Encrypt(key.c_str());
            if (enc) { config["xaiApiKey"] = std::string(enc); PluginKeyEncryption_FreeString(enc); }
            else { config["xaiApiKey"] = key; }
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
            PLUGIN_LOG(PLUGIN_LOG_ERROR, "[AIConfigPanel] Failed to open config file for writing");
            return;
        }

        file << config.dump(2);
        file.close();
        PLUGIN_LOG(PLUGIN_LOG_INFO, "[AIConfigPanel] Saved plugin config to: {}", configPath.string());
        // Apply saved settings to runtime plugin-local config
        try {
            config::GetConfig().ApplyJson(config);
            PLUGIN_LOG(PLUGIN_LOG_DEBUG, "[AIConfigPanel] Applied plugin config to runtime");
        } catch (...) {
            PLUGIN_LOG(PLUGIN_LOG_WARN, "[AIConfigPanel] Failed to apply plugin config to runtime");
        }
    }
    catch (const std::exception& ex)
    {
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "[AIConfigPanel] Failed to save plugin config: {}", ex.what());
    }
}

} // namespace ui::qt
