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
 * @brief Widget for SSH connection configuration
 * 
 * This widget provides connection and authentication configuration
 * for SSH connections. It's designed to be placed in the plugin
 * configuration dock.
 */
class SSHConnectionWidget : public QWidget
{
    Q_OBJECT

    // cppcheck-suppress unknownMacro
public:
    explicit SSHConnectionWidget(QWidget* parent = nullptr);
    ~SSHConnectionWidget() override;
    
    /**
     * @brief Load configuration from plugin config file
     */
    void LoadConfiguration();
    
    /**
     * @brief Save configuration to plugin config file
     */
    void SaveConfiguration();
    
    /**
     * @brief Get current connection configuration
     */
    SSHConnectionConfig GetConnectionConfig() const;

signals:
    /**
     * @brief Emitted when connection is requested
     * @param config Connection configuration to use
     */
    void connectionRequested(const SSHConnectionConfig& config);
    
    /**
     * @brief Emitted when disconnection is requested
     */
    void disconnectionRequested();
    
    /**
     * @brief Emitted when configuration changes
     */
    void configurationChanged();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onTestConnectionClicked();
    void onAuthMethodChanged(int index);

private:
    void setupUI();
    void setupConnectionGroup();
    void setupAuthenticationGroup();
    void setupControlButtons();

    // UI Components - Connection
    QLineEdit* m_hostnameEdit;
    QSpinBox* m_portSpinBox;
    QLineEdit* m_usernameEdit;
    
    // UI Components - Authentication
    QComboBox* m_authMethodCombo;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_privateKeyPathEdit;
    QPushButton* m_browseKeyButton;
    
    // UI Components - Controls
    QPushButton* m_connectButton;
    QPushButton* m_disconnectButton;
    QPushButton* m_testButton;
};

} // namespace ssh
