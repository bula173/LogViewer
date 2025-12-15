/**
 * @file SSHTerminalWidget.hpp
 * @brief Widget for SSH terminal console output and input
 * @author LogViewer Development Team
 * @date 2025
 */

#pragma once

#include "SSHConnection.hpp"
#include "SSHLogSource.hpp"
#include "parser/IDataParser.hpp"
#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <memory>

class QVBoxLayout;

namespace ssh
{

/**
 * @brief Widget for SSH terminal console with output and command input
 * 
 * This widget displays the terminal output and provides command input
 * for interactive SSH sessions. It's designed to be placed in the main
 * panel of the application.
 */
class SSHTerminalWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SSHTerminalWidget(QWidget* parent = nullptr);
    ~SSHTerminalWidget() override;

    /**
     * @brief Sets the data parser observer for streaming events
     * @param observer Observer to receive parsed events
     */
    void SetParserObserver(parser::IDataParserObserver* observer);
    
    /**
     * @brief Set the SSH connection for this terminal
     * @param connection SSH connection to use
     * @param config Connection configuration
     */
    void SetConnection(std::unique_ptr<SSHConnection> connection, const SSHConnectionConfig& config);
    
    /**
     * @brief Set the log source for monitoring
     * @param logSource SSH log source to use
     */
    void SetLogSource(std::unique_ptr<SSHLogSource> logSource);
    
    /**
     * @brief Start the terminal session or log monitoring
     */
    void Start();
    
    /**
     * @brief Stop the terminal session or log monitoring
     */
    void Stop();
    
    /**
     * @brief Check if terminal is currently active
     */
    bool IsActive() const { return m_isActive; }

signals:
    void statusChanged(const QString& status, bool isActive);

private slots:
    void onCommandEntered();
    void onModeChanged(int index);

private:
    void setupUI();
    void setupControlsGroup();
    void setupConsoleGroup();
    
    void appendLog(const QString& message);
    void updateStatus(const QString& status, bool isActive);

    // UI Components
    QComboBox* m_modeCombo;
    QLineEdit* m_commandEdit;
    QLineEdit* m_logFilePathEdit;
    QTextEdit* m_consoleView;
    QLineEdit* m_commandInput;
    QPushButton* m_sendCommandButton;
    QLabel* m_statusLabel;
    
    // SSH Components
    std::unique_ptr<SSHLogSource> m_logSource;
    std::unique_ptr<SSHConnection> m_terminalConnection;
    SSHConnectionConfig m_connectionConfig;
    parser::IDataParserObserver* m_parserObserver;
    bool m_isActive;
};

} // namespace ssh
