/**
 * @file EventMetricsConfigWidget.hpp
 * @brief Configuration widget for event metrics filter rules
 * @author LogViewer Development Team
 * @date 2025
 */

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <vector>
#include <string>

namespace plugin
{

/**
 * @brief Configuration widget for event metrics
 */
class EventMetricsConfigWidget : public QWidget
{
    Q_OBJECT

    // cppcheck-suppress unknownMacro
public:
    explicit EventMetricsConfigWidget(QWidget* parent = nullptr);

    struct MetricRule {
        std::string name;
        std::string typePattern;  // Event type pattern (regex)
        std::string severity;     // "Error", "Warning", "Info", "Critical"
        bool enabled;
    };

    std::vector<MetricRule> getRules() const { return m_rules; }

signals:
    void rulesChanged();

    // cppcheck-suppress unknownMacro
private slots:
    void onAddRuleClicked();
    void onRemoveRuleClicked();
    void onToggleRuleClicked();

private:
    void setupUI();
    void loadDefaultRules();
    void updateRulesList();
    
    QLineEdit* m_nameEdit;
    QLineEdit* m_patternEdit;
    QComboBox* m_severityCombo;
    QListWidget* m_rulesList;
    std::vector<MetricRule> m_rules;
};

} // namespace plugin
