/**
 * @file SSHConnectionWidget.cpp
 * @brief Implementation of SSH connection widget
 * @author LogViewer Development Team
 * @date 2025
 */

#include "SSHConnectionWidget.hpp"
#include "SSHTextParser.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>

namespace ssh
{

SSHConnectionWidget::SSHConnectionWidget(QWidget* parent)
    : QWidget(parent)
    , m_parserObserver(nullptr)
    , m_isConnected(false)
{
    setupUI();
}

SSHConnectionWidget::~SSHConnectionWidget()
{
    if (m_isConnected) {
        stopLogMonitoring();
    }
}

void SSHConnectionWidget::SetParserObserver(parser::IDataParserObserver* observer)
{
    m_parserObserver = observer;
}

void SSHConnectionWidget::setupUI()
{
    new QVBoxLayout(this);

    setupConnectionGroup();
    setupAuthenticationGroup();
    setupLogSourceGroup();
    setupControlButtons();
    setupLogViewGroup();
}

void SSHConnectionWidget::setupConnectionGroup()
{
    auto* group = new QGroupBox("SSH Connection", this);
    auto* layout = new QVBoxLayout();
    
    auto* hostLayout = new QHBoxLayout();
    hostLayout->addWidget(new QLabel("Hostname:", this));
    m_hostnameEdit = new QLineEdit(this);
    m_hostnameEdit->setPlaceholderText("e.g., server.example.com");
    hostLayout->addWidget(m_hostnameEdit);
    
    hostLayout->addWidget(new QLabel("Port:", this));
    m_portSpinBox = new QSpinBox(this);
    m_portSpinBox->setRange(1, 65535);
    m_portSpinBox->setValue(22);
    hostLayout->addWidget(m_portSpinBox);
    layout->addLayout(hostLayout);
    
    auto* userLayout = new QHBoxLayout();
    userLayout->addWidget(new QLabel("Username:", this));
    m_usernameEdit = new QLineEdit(this);
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
    keyLayout->addWidget(m_privateKeyPathEdit);
    
    m_browseKeyButton = new QPushButton("Browse...", this);
    m_browseKeyButton->setEnabled(false);
    keyLayout->addWidget(m_browseKeyButton);
    layout->addLayout(keyLayout);
    
    group->setLayout(layout);
    
    auto* parentLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (parentLayout) {
        parentLayout->addWidget(group);
    }
}

void SSHConnectionWidget::setupLogSourceGroup()
{
    auto* group = new QGroupBox("Log Source", this);
    auto* layout = new QVBoxLayout();
    
    auto* modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel("Mode:", this));
    m_logSourceModeCombo = new QComboBox(this);
    m_logSourceModeCombo->addItems({"Execute Command", "Monitor File"});
    modeLayout->addWidget(m_logSourceModeCombo);
    layout->addLayout(modeLayout);
    
    auto* commandLayout = new QHBoxLayout();
    commandLayout->addWidget(new QLabel("Command:", this));
    m_commandEdit = new QLineEdit(this);
    m_commandEdit->setPlaceholderText("e.g., tail -f /var/log/syslog");
    commandLayout->addWidget(m_commandEdit);
    layout->addLayout(commandLayout);
    
    auto* fileLayout = new QHBoxLayout();
    fileLayout->addWidget(new QLabel("Log File:", this));
    m_logFilePathEdit = new QLineEdit(this);
    m_logFilePathEdit->setPlaceholderText("/var/log/application.log");
    m_logFilePathEdit->setEnabled(false);
    fileLayout->addWidget(m_logFilePathEdit);
    layout->addLayout(fileLayout);
    
    group->setLayout(layout);
    
    auto* parentLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (parentLayout) {
        parentLayout->addWidget(group);
    }
}

void SSHConnectionWidget::setupControlButtons()
{
    auto* layout = new QHBoxLayout();
    
    m_statusLabel = new QLabel("Not connected", this);
    layout->addWidget(m_statusLabel);
    layout->addStretch();
    
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

void SSHConnectionWidget::setupLogViewGroup()
{
    auto* group = new QGroupBox("Live Log Stream", this);
    auto* layout = new QVBoxLayout();
    
    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMinimumHeight(300);
    layout->addWidget(m_logView);
    
    group->setLayout(layout);
    
    auto* parentLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (parentLayout) {
        parentLayout->addWidget(group);
    }
}

void SSHConnectionWidget::onAuthMethodChanged(int index)
{
    bool isPublicKey = (index == 1);
    m_privateKeyPathEdit->setEnabled(isPublicKey);
    m_browseKeyButton->setEnabled(isPublicKey);
    m_passwordEdit->setEnabled(!isPublicKey);
}

void SSHConnectionWidget::onConnectClicked()
{
    QString hostname = m_hostnameEdit->text();
    if (hostname.isEmpty()) {
        QMessageBox::warning(this, "Connection Error", "Please enter a hostname");
        return;
    }
    
    updateConnectionStatus("Connecting...", false);
    appendLog(QString("Attempting to connect to %1:%2...")
        .arg(hostname).arg(m_portSpinBox->value()));
    
    // TODO: Implement actual SSH connection
    appendLog("SSH connection not yet implemented");
    updateConnectionStatus("Not implemented", false);
}

void SSHConnectionWidget::onDisconnectClicked()
{
    stopLogMonitoring();
    m_logSource.reset();
    m_isConnected = false;
    updateConnectionStatus("Disconnected", false);
    appendLog("Disconnected from server");
}

void SSHConnectionWidget::onTestConnectionClicked()
{
    QString hostname = m_hostnameEdit->text();
    if (hostname.isEmpty()) {
        QMessageBox::warning(this, "Test Error", "Please enter a hostname");
        return;
    }
    
    appendLog("Testing connection...");
    appendLog("Connection test not yet implemented");
    QMessageBox::information(this, "Test Result", "Connection test not yet implemented");
}

void SSHConnectionWidget::updateConnectionStatus(const QString& status, bool isConnected)
{
    m_statusLabel->setText(status);
    m_connectButton->setEnabled(!isConnected);
    m_disconnectButton->setEnabled(isConnected);
    m_testButton->setEnabled(!isConnected);
}

void SSHConnectionWidget::appendLog(const QString& message)
{
    m_logView->append(QString("[%1] %2")
        .arg(QTime::currentTime().toString("HH:mm:ss"))
        .arg(message));
}

void SSHConnectionWidget::startLogMonitoring()
{
    appendLog("Starting log monitoring...");
    appendLog("Log monitoring not yet implemented");
}

void SSHConnectionWidget::stopLogMonitoring()
{
    if (m_logSource) {
        m_logSource->Stop();
        appendLog("Log monitoring stopped");
    }
}

} // namespace ssh

