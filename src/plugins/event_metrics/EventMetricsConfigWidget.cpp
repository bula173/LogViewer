/**
 * @file EventMetricsConfigWidget.cpp
 * @brief Implementation of event metrics configuration widget
 * @author LogViewer Development Team
 * @date 2025
 */

#include "EventMetricsConfigWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

namespace plugin
{

EventMetricsConfigWidget::EventMetricsConfigWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    loadDefaultRules();
}

void EventMetricsConfigWidget::onAddRuleClicked()
{
    QString name = m_nameEdit->text();
    QString pattern = m_patternEdit->text();
    QString severity = m_severityCombo->currentText();
    
    if (name.isEmpty() || pattern.isEmpty()) {
        return;
    }
    
    MetricRule rule;
    rule.name = name.toStdString();
    rule.typePattern = pattern.toStdString();
    rule.severity = severity.toStdString();
    rule.enabled = true;
    
    m_rules.push_back(rule);
    updateRulesList();
    
    m_nameEdit->clear();
    m_patternEdit->clear();
    
    emit rulesChanged();
}

void EventMetricsConfigWidget::onRemoveRuleClicked()
{
    int currentRow = m_rulesList->currentRow();
    if (currentRow >= 0 && currentRow < static_cast<int>(m_rules.size())) {
        m_rules.erase(m_rules.begin() + currentRow);
        updateRulesList();
        emit rulesChanged();
    }
}

void EventMetricsConfigWidget::onToggleRuleClicked()
{
    int currentRow = m_rulesList->currentRow();
    if (currentRow >= 0 && currentRow < static_cast<int>(m_rules.size())) {
        m_rules[static_cast<size_t>(currentRow)].enabled = !m_rules[static_cast<size_t>(currentRow)].enabled;
        updateRulesList();
        emit rulesChanged();
    }
}

void EventMetricsConfigWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    
    // Description
    auto* descLabel = new QLabel(
        "Configure which event types to track in metrics. "
        "Use regex patterns to match event types and classify them by severity.", this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);
    
    // Add rule section
    auto* addGroup = new QGroupBox("Add Metric Rule", this);
    auto* addLayout = new QVBoxLayout();
    
    auto* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("Rule Name:", this));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("e.g., 'Track Errors'");
    nameLayout->addWidget(m_nameEdit);
    addLayout->addLayout(nameLayout);
    
    auto* patternLayout = new QHBoxLayout();
    patternLayout->addWidget(new QLabel("Type Pattern:", this));
    m_patternEdit = new QLineEdit(this);
    m_patternEdit->setPlaceholderText("e.g., '.*error.*' or 'ERROR|FATAL'");
    patternLayout->addWidget(m_patternEdit);
    addLayout->addLayout(patternLayout);
    
    auto* severityLayout = new QHBoxLayout();
    severityLayout->addWidget(new QLabel("Severity:", this));
    m_severityCombo = new QComboBox(this);
    m_severityCombo->addItems({"Critical", "Error", "Warning", "Info"});
    severityLayout->addWidget(m_severityCombo);
    severityLayout->addStretch();
    addLayout->addLayout(severityLayout);
    
    auto* addButton = new QPushButton("Add Rule", this);
    connect(addButton, &QPushButton::clicked, this, &EventMetricsConfigWidget::onAddRuleClicked);
    addLayout->addWidget(addButton);
    
    addGroup->setLayout(addLayout);
    mainLayout->addWidget(addGroup);
    
    // Rules list section
    auto* rulesGroup = new QGroupBox("Configured Rules", this);
    auto* rulesLayout = new QVBoxLayout();
    
    m_rulesList = new QListWidget(this);
    rulesLayout->addWidget(m_rulesList);
    
    auto* buttonLayout = new QHBoxLayout();
    auto* toggleButton = new QPushButton("Enable/Disable", this);
    connect(toggleButton, &QPushButton::clicked, this, &EventMetricsConfigWidget::onToggleRuleClicked);
    buttonLayout->addWidget(toggleButton);
    
    auto* removeButton = new QPushButton("Remove", this);
    connect(removeButton, &QPushButton::clicked, this, &EventMetricsConfigWidget::onRemoveRuleClicked);
    buttonLayout->addWidget(removeButton);
    buttonLayout->addStretch();
    
    rulesLayout->addLayout(buttonLayout);
    rulesGroup->setLayout(rulesLayout);
    mainLayout->addWidget(rulesGroup);
    
    mainLayout->addStretch();
}

void EventMetricsConfigWidget::loadDefaultRules()
{
    m_rules = {
        {"Critical Errors", "CRITICAL|FATAL|PANIC", "Critical", true},
        {"Errors", "ERROR", "Error", true},
        {"Warnings", "WARNING|WARN", "Warning", true},
        {"Info Events", "INFO|INFORMATION", "Info", true}
    };
    updateRulesList();
}

void EventMetricsConfigWidget::updateRulesList()
{
    m_rulesList->clear();
    for (const auto& rule : m_rules) {
        QString status = rule.enabled ? "✓" : "✗";
        QString text = QString("%1 [%2] %3: %4")
            .arg(status)
            .arg(QString::fromStdString(rule.severity))
            .arg(QString::fromStdString(rule.name))
            .arg(QString::fromStdString(rule.typePattern));
        m_rulesList->addItem(text);
    }
}

} // namespace plugin

