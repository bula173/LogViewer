/**
 * @file SSHConnectionWidget.cpp
 * @brief Implementation of SSH connection widget
 * @author LogViewer Development Team
 * @date 2025
 */

#include "SSHConnectionWidget.hpp"
#include "SSHTextParser.hpp"
#include "plugins/PluginManager.hpp"
#include "util/Logger.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QTime>
#include <QDir>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace ssh
{

SSHConnectionWidget::SSHConnectionWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setupUI();
    LoadConfiguration();
}

SSHConnectionWidget::~SSHConnectionWidget()
{
}

void SSHConnectionWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    setupConnectionGroup();
    setupAuthenticationGroup();
    setupControlButtons();
    
    mainLayout->addStretch();  // Push content to top
}

void SSHConnectionWidget::setupConnectionGroup()
{
    auto* group = new QGroupBox("SSH Connection", this);
    auto* layout = new QVBoxLayout();
    
    auto* hostLayout = new QHBoxLayout();
    hostLayout->addWidget(new QLabel("Hostname:", this));
    m_hostnameEdit = new QLineEdit(this);
    m_hostnameEdit->setPlaceholderText("e.g., server.example.com");
    connect(m_hostnameEdit, &QLineEdit::editingFinished, this, &SSHConnectionWidget::SaveConfiguration);
    hostLayout->addWidget(m_hostnameEdit);
    
    hostLayout->addWidget(new QLabel("Port:", this));
    m_portSpinBox = new QSpinBox(this);
    m_portSpinBox->setRange(1, 65535);
    m_portSpinBox->setValue(22);
    connect(m_portSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &SSHConnectionWidget::SaveConfiguration);
    hostLayout->addWidget(m_portSpinBox);
    layout->addLayout(hostLayout);
    
    auto* userLayout = new QHBoxLayout();
    userLayout->addWidget(new QLabel("Username:", this));
    m_usernameEdit = new QLineEdit(this);
    connect(m_usernameEdit, &QLineEdit::editingFinished, this, &SSHConnectionWidget::SaveConfiguration);
    userLayout->addWidget(m_usernameEdit);
    layout->addLayout(userLayout);
    
    group->setLayout(layout);
    
    auto* parentLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (parentLayout) {
        parentLayout->addWidget(group);
    }
}

void SSHConnectionWidget::setupAuthenticationGroup()
{
    auto* group = new QGroupBox("Authentication", this);
    auto* layout = new QVBoxLayout();
    
    auto* methodLayout = new QHBoxLayout();
    methodLayout->addWidget(new QLabel("Method:", this));
    m_authMethodCombo = new QComboBox(this);
    m_authMethodCombo->addItems({"Password", "Public Key"});
    connect(m_authMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SSHConnectionWidget::onAuthMethodChanged);
    methodLayout->addWidget(m_authMethodCombo);
    layout->addLayout(methodLayout);
    
    auto* passwordLayout = new QHBoxLayout();
    passwordLayout->addWidget(new QLabel("Password:", this));
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    passwordLayout->addWidget(m_passwordEdit);
    layout->addLayout(passwordLayout);
    
    auto* keyLayout = new QHBoxLayout();
    keyLayout->addWidget(new QLabel("Private Key:", this));
    m_privateKeyPathEdit = new QLineEdit(this);
    m_privateKeyPathEdit->setPlaceholderText("~/.ssh/id_rsa");
    m_privateKeyPathEdit->setEnabled(false);
    connect(m_privateKeyPathEdit, &QLineEdit::editingFinished, this, &SSHConnectionWidget::SaveConfiguration);
    keyLayout->addWidget(m_privateKeyPathEdit);
    
    m_browseKeyButton = new QPushButton("Browse...", this);
    m_browseKeyButton->setEnabled(false);
    connect(m_browseKeyButton, &QPushButton::clicked, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Select Private Key"), 
                                                        QDir::homePath() + "/.ssh",
                                                        tr("Key Files (*)"));
        if (!fileName.isEmpty()) {
            m_privateKeyPathEdit->setText(fileName);
            SaveConfiguration();
        }
    });
    keyLayout->addWidget(m_browseKeyButton);
    layout->addLayout(keyLayout);
    
    group->setLayout(layout);
    
    auto* parentLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (parentLayout) {
        parentLayout->addWidget(group);
    }
}

void SSHConnectionWidget::setupControlButtons()
{
    auto* layout = new QHBoxLayout();
    
    m_testButton = new QPushButton("Test Connection", this);
    connect(m_testButton, &QPushButton::clicked, this, &SSHConnectionWidget::onTestConnectionClicked);
    layout->addWidget(m_testButton);
    
    m_connectButton = new QPushButton("Connect", this);
    connect(m_connectButton, &QPushButton::clicked, this, &SSHConnectionWidget::onConnectClicked);
    layout->addWidget(m_connectButton);
    
    m_disconnectButton = new QPushButton("Disconnect", this);
    m_disconnectButton->setEnabled(false);
    connect(m_disconnectButton, &QPushButton::clicked, this, &SSHConnectionWidget::onDisconnectClicked);
    layout->addWidget(m_disconnectButton);
    
    auto* parentLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (parentLayout) {
        parentLayout->addLayout(layout);
    }
}

void SSHConnectionWidget::onAuthMethodChanged(int index)
{
    bool isPublicKey = (index == 1);
    m_privateKeyPathEdit->setEnabled(isPublicKey);
    m_browseKeyButton->setEnabled(isPublicKey);
    m_passwordEdit->setEnabled(!isPublicKey);
}

SSHConnectionConfig SSHConnectionWidget::GetConnectionConfig() const
{
    SSHConnectionConfig config;
    config.hostname = m_hostnameEdit->text().toStdString();
    config.port = m_portSpinBox->value();
    config.username = m_usernameEdit->text().toStdString();
    config.timeoutSeconds = 30;
    config.strictHostKeyChecking = false;
    
    if (m_authMethodCombo->currentIndex() == 0) {
        config.authMethod = SSHAuthMethod::Password;
        config.password = m_passwordEdit->text().toStdString();
    } else {
        config.authMethod = SSHAuthMethod::PublicKey;
        config.privateKeyPath = m_privateKeyPathEdit->text().toStdString();
    }
    
    return config;
}

void SSHConnectionWidget::onConnectClicked()
{
    QString hostname = m_hostnameEdit->text();
    if (hostname.isEmpty()) {
        QMessageBox::warning(this, "Connection Error", "Please enter a hostname");
        return;
    }
    
    QString username = m_usernameEdit->text();
    if (username.isEmpty()) {
        QMessageBox::warning(this, "Connection Error", "Please enter a username");
        return;
    }
    
    // Validate authentication
    if (m_authMethodCombo->currentIndex() == 0) {
        if (m_passwordEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Connection Error", "Please enter a password");
            return;
        }
    } else {
        if (m_privateKeyPathEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Connection Error", "Please select a private key file");
            return;
        }
    }
    
    // Get configuration and emit signal
    SSHConnectionConfig config = GetConnectionConfig();
    emit connectionRequested(config);
    
    m_connectButton->setEnabled(false);
    m_disconnectButton->setEnabled(true);
}

void SSHConnectionWidget::onDisconnectClicked()
{
    emit disconnectionRequested();
    
    m_connectButton->setEnabled(true);
    m_disconnectButton->setEnabled(false);
}

void SSHConnectionWidget::onTestConnectionClicked()
{
    QString hostname = m_hostnameEdit->text();
    if (hostname.isEmpty()) {
        QMessageBox::warning(this, "Test Error", "Please enter a hostname");
        return;
    }
    
    QString username = m_usernameEdit->text();
    if (username.isEmpty()) {
        QMessageBox::warning(this, "Test Error", "Please enter a username");
        return;
    }
    
    // Validate authentication
    if (m_authMethodCombo->currentIndex() == 0) {
        if (m_passwordEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Test Error", "Please enter a password");
            return;
        }
    } else {
        if (m_privateKeyPathEdit->text().isEmpty()) {
            QMessageBox::warning(this, "Test Error", "Please select a private key file");
            return;
        }
    }
    
    // Get configuration and test
    SSHConnectionConfig config = GetConnectionConfig();
    config.timeoutSeconds = 10;  // Shorter timeout for test
    
    try {
        SSHConnection testConnection(config);
        
        if (testConnection.Connect()) {
            QMessageBox::information(this, "Test Result", 
                "Connection test successful!\n\nSSH connection to " + hostname + " is working.");
            testConnection.Disconnect();
        } else {
            QString error = QString::fromStdString(testConnection.GetLastError());
            QMessageBox::warning(this, "Test Result", 
                "Connection test failed:\n\n" + error);
        }
    }
    catch (const std::exception& e) {
        QMessageBox::warning(this, "Test Result", 
            QString("Connection test error:\n\n%1").arg(e.what()));
    }
}

void SSHConnectionWidget::LoadConfiguration()
{
    auto& pluginManager = plugin::PluginManager::GetInstance();
    std::filesystem::path configPath = pluginManager.GetPluginConfigPath("ssh_parser");
    
    if (!std::filesystem::exists(configPath))
    {
        util::Logger::Debug("[SSHConnectionWidget] No plugin config file found, using defaults");
        return;
    }

    try
    {
        std::ifstream file(configPath);
        if (!file.is_open())
        {
            util::Logger::Warn("[SSHConnectionWidget] Failed to open plugin config file");
            return;
        }

        nlohmann::json config;
        file >> config;
        
        // Load connection settings
        if (config.contains("hostname"))
            m_hostnameEdit->setText(QString::fromStdString(config["hostname"]));
        if (config.contains("port"))
            m_portSpinBox->setValue(config["port"]);
        if (config.contains("username"))
            m_usernameEdit->setText(QString::fromStdString(config["username"]));
        
        // Load authentication settings
        if (config.contains("authMethod"))
        {
            QString authMethod = QString::fromStdString(config["authMethod"]);
            int index = m_authMethodCombo->findText(authMethod);
            if (index >= 0)
                m_authMethodCombo->setCurrentIndex(index);
        }
        if (config.contains("privateKeyPath"))
            m_privateKeyPathEdit->setText(QString::fromStdString(config["privateKeyPath"]));
        
        util::Logger::Info("[SSHConnectionWidget] Configuration loaded");
        emit configurationChanged();
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[SSHConnectionWidget] Failed to load plugin config: {}", ex.what());
    }
}

void SSHConnectionWidget::SaveConfiguration()
{
    auto& pluginManager = plugin::PluginManager::GetInstance();
    std::filesystem::path configPath = pluginManager.GetPluginConfigPath("ssh_parser");
    
    try
    {
        // Ensure parent directory exists
        std::filesystem::create_directories(configPath.parent_path());
        
        nlohmann::json config;
        
        // Save connection settings
        config["hostname"] = m_hostnameEdit->text().toStdString();
        config["port"] = m_portSpinBox->value();
        config["username"] = m_usernameEdit->text().toStdString();
        
        // Save authentication settings
        config["authMethod"] = m_authMethodCombo->currentText().toStdString();
        config["privateKeyPath"] = m_privateKeyPathEdit->text().toStdString();
        // Note: Password is not saved for security reasons
        
        std::ofstream file(configPath);
        if (!file.is_open())
        {
            util::Logger::Error("[SSHConnectionWidget] Failed to open config file for writing");
            return;
        }
        
        file << config.dump(4);
        util::Logger::Info("[SSHConnectionWidget] Configuration saved");
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[SSHConnectionWidget] Failed to save plugin config: {}", ex.what());
    }
}

} // namespace ssh

