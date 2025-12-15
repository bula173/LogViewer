/**
 * @file SSHTerminalWidget.cpp
 * @brief Implementation of SSH terminal console widget
 * @author LogViewer Development Team
 * @date 2025
 */

#include "SSHTerminalWidget.hpp"
#include "SSHTextParser.hpp"
#include "util/Logger.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QTime>

namespace ssh
{

SSHTerminalWidget::SSHTerminalWidget(QWidget* parent)
    : QWidget(parent)
    , m_parserObserver(nullptr)
    , m_isActive(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setupUI();
}

SSHTerminalWidget::~SSHTerminalWidget()
{
    Stop();
}

void SSHTerminalWidget::SetParserObserver(parser::IDataParserObserver* observer)
{
    m_parserObserver = observer;
}

void SSHTerminalWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    setupControlsGroup();
    setupConsoleGroup();
}

void SSHTerminalWidget::setupControlsGroup()
{
    auto* group = new QGroupBox("Log Source Configuration", this);
    auto* layout = new QVBoxLayout();
    
    // Mode selector
    auto* modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel("Mode:", this));
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItems({"Execute Command", "Monitor File", "Interactive Terminal"});
    m_modeCombo->setCurrentIndex(2);  // Default to Interactive Terminal
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SSHTerminalWidget::onModeChanged);
    modeLayout->addWidget(m_modeCombo);
    modeLayout->addStretch();
    layout->addLayout(modeLayout);
    
    // Command input
    auto* commandLayout = new QHBoxLayout();
    commandLayout->addWidget(new QLabel("Command:", this));
    m_commandEdit = new QLineEdit(this);
    m_commandEdit->setPlaceholderText("Not used in interactive mode");
    m_commandEdit->setEnabled(false);
    commandLayout->addWidget(m_commandEdit);
    layout->addLayout(commandLayout);
    
    // File path input
    auto* fileLayout = new QHBoxLayout();
    fileLayout->addWidget(new QLabel("Log File Path:", this));
    m_logFilePathEdit = new QLineEdit(this);
    m_logFilePathEdit->setPlaceholderText("Not used in interactive mode");
    m_logFilePathEdit->setEnabled(false);
    fileLayout->addWidget(m_logFilePathEdit);
    layout->addLayout(fileLayout);
    
    // Status
    m_statusLabel = new QLabel("Not connected", this);
    m_statusLabel->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    layout->addWidget(m_statusLabel);
    
    group->setLayout(layout);
    
    auto* parentLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (parentLayout) {
        parentLayout->addWidget(group);
    }
}

void SSHTerminalWidget::setupConsoleGroup()
{
    auto* group = new QGroupBox("Console Output", this);
    auto* layout = new QVBoxLayout();
    
    // Console output view
    m_consoleView = new QTextEdit(this);
    m_consoleView->setReadOnly(true);
    m_consoleView->setFont(QFont("Courier", 10));
    m_consoleView->setMinimumHeight(400);
    layout->addWidget(m_consoleView);
    
    // Terminal command input (visible in interactive mode)
    auto* commandLayout = new QHBoxLayout();
    commandLayout->addWidget(new QLabel("Command:", this));
    m_commandInput = new QLineEdit(this);
    m_commandInput->setPlaceholderText("Enter command to execute on remote host...");
    m_commandInput->setEnabled(false);
    connect(m_commandInput, &QLineEdit::returnPressed, this, &SSHTerminalWidget::onCommandEntered);
    commandLayout->addWidget(m_commandInput);
    
    m_sendCommandButton = new QPushButton("Send", this);
    m_sendCommandButton->setEnabled(false);
    connect(m_sendCommandButton, &QPushButton::clicked, this, &SSHTerminalWidget::onCommandEntered);
    commandLayout->addWidget(m_sendCommandButton);
    layout->addLayout(commandLayout);
    
    group->setLayout(layout);
    
    auto* parentLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (parentLayout) {
        parentLayout->addWidget(group);
    }
}

void SSHTerminalWidget::SetConnection(std::unique_ptr<SSHConnection> connection, 
                                      const SSHConnectionConfig& config)
{
    m_terminalConnection = std::move(connection);
    m_connectionConfig = config;
}

void SSHTerminalWidget::SetLogSource(std::unique_ptr<SSHLogSource> logSource)
{
    m_logSource = std::move(logSource);
}

void SSHTerminalWidget::Start()
{
    int mode = m_modeCombo->currentIndex();
    
    if (mode == 2) {
        // Interactive Terminal mode
        if (m_terminalConnection) {
            m_isActive = true;
            updateStatus("Connected (Terminal Mode)", true);
            appendLog("Terminal connection established");
            appendLog("Type commands below and press Enter to execute");
            
            // Enable terminal input
            m_commandInput->setEnabled(true);
            m_sendCommandButton->setEnabled(true);
            m_commandInput->setFocus();
        }
    } else {
        // Log source modes
        if (m_logSource && m_logSource->Start()) {
            m_isActive = true;
            updateStatus("Connected", true);
            appendLog("Log monitoring started");
            
            // TODO: Start polling log source for data
            // This would typically involve a timer or background thread
        } else {
            QString error = m_logSource ? 
                QString::fromStdString(m_logSource->GetLastError()) : 
                "No log source available";
            appendLog(QString("Failed to start: %1").arg(error));
            updateStatus("Connection failed", false);
        }
    }
}

void SSHTerminalWidget::Stop()
{
    if (!m_isActive) {
        return;
    }
    
    if (m_logSource) {
        m_logSource->Stop();
        m_logSource.reset();
    }
    
    if (m_terminalConnection) {
        m_terminalConnection->Disconnect();
        m_terminalConnection.reset();
        m_commandInput->setEnabled(false);
        m_sendCommandButton->setEnabled(false);
    }
    
    m_isActive = false;
    updateStatus("Disconnected", false);
    appendLog("Disconnected from server");
}

void SSHTerminalWidget::onCommandEntered()
{
    if (!m_isActive || !m_terminalConnection) {
        appendLog("Error: Not connected to remote host");
        return;
    }
    
    QString command = m_commandInput->text().trimmed();
    if (command.isEmpty()) {
        return;
    }
    
    appendLog(QString("$ %1").arg(command));
    m_commandInput->clear();
    
    // Execute command on remote host
    // TODO: Implement command execution through SSH connection
    appendLog("Command execution will be implemented...");
    
    // For now, simulate command execution
    // In a real implementation, this would use SSHConnection to execute the command
    // and read the output back
}

void SSHTerminalWidget::onModeChanged(int index)
{
    // 0 = Execute Command, 1 = Monitor File, 2 = Interactive Terminal
    bool isCommand = (index == 0);
    bool isFile = (index == 1);
    bool isTerminal = (index == 2);
    
    m_commandEdit->setEnabled(isCommand);
    m_logFilePathEdit->setEnabled(isFile);
    
    // Update UI hints
    if (isTerminal) {
        m_commandEdit->setPlaceholderText("Not used in interactive mode");
        m_logFilePathEdit->setPlaceholderText("Not used in interactive mode");
    } else if (isCommand) {
        m_commandEdit->setPlaceholderText("e.g., tail -f /var/log/syslog");
        m_logFilePathEdit->setPlaceholderText("Not used in command mode");
    } else {
        m_commandEdit->setPlaceholderText("Not used in file monitor mode");
        m_logFilePathEdit->setPlaceholderText("/var/log/application.log");
    }
}

void SSHTerminalWidget::appendLog(const QString& message)
{
    QString timestamp = QTime::currentTime().toString("HH:mm:ss");
    m_consoleView->append(QString("[%1] %2").arg(timestamp, message));
}

void SSHTerminalWidget::updateStatus(const QString& status, bool isActive)
{
    m_statusLabel->setText(status);
    
    if (isActive) {
        m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    } else {
        m_statusLabel->setStyleSheet("QLabel { color: gray; font-weight: bold; }");
    }
    
    emit statusChanged(status, isActive);
}

} // namespace ssh
