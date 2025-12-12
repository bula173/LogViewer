/**
 * @file SSHConnectionWidget.hpp
 * @brief Widget for SSH connection configuration and live log viewing
 * @author LogViewer Development Team
 * @date 2025
 */

#pragma once

#include "SSHConnection.hpp"
#include "SSHLogSource.hpp"
#include "parser/IDataParser.hpp"
#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QLabel>
#include <memory>

class QVBoxLayout;
class QHBoxLayout;
class QGroupBox;

namespace ssh
{

/**
 * @brief Widget for managing SSH connections and viewing live logs
 */
class SSHConnectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SSHConnectionWidget(QWidget* parent = nullptr);
    ~SSHConnectionWidget() override;

    /**
     * @brief Sets the data parser for streaming events
     * @param observer Observer to receive parsed events
     */
    void SetParserObserver(parser::IDataParserObserver* observer);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onTestConnectionClicked();
    void onAuthMethodChanged(int index);

private:
    void setupUI();
    void setupConnectionGroup();
    void setupAuthenticationGroup();
    void setupLogSourceGroup();
    void setupLogViewGroup();
    void setupControlButtons();
    
    void updateConnectionStatus(const QString& status, bool isConnected);
    void appendLog(const QString& message);
    void startLogMonitoring();
    void stopLogMonitoring();

    // UI Components - Connection
    QLineEdit* m_hostnameEdit;
    QSpinBox* m_portSpinBox;
    QLineEdit* m_usernameEdit;
    
    // UI Components - Authentication
    QComboBox* m_authMethodCombo;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_privateKeyPathEdit;
    QPushButton* m_browseKeyButton;
    
    // UI Components - Log Source
    QComboBox* m_logSourceModeCombo;
    QLineEdit* m_commandEdit;
    QLineEdit* m_logFilePathEdit;
    
    // UI Components - Status and Controls
    QLabel* m_statusLabel;
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QPushButton* m_testButton;
    QTextEdit* m_logView;
    
    // SSH Components
    std::unique_ptr<SSHLogSource> m_logSource;
    parser::IDataParserObserver* m_parserObserver;
    bool m_isConnected;
};

} // namespace ssh
