#pragma once

#include <QWidget>
#include <QString>
#include <memory>
#include <string>

class QTableWidget;

class MetricsEngine;

class ConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigWidget(QWidget* parent = nullptr);
    void SetEngine(MetricsEngine* engine); // cppcheck-suppress unknownMacro

signals:
    void configChanged();

    // cppcheck-suppress unknownMacro
public slots:
    void LoadFromFile(const QString& path);
    void SaveToFile(const QString& path);

private:
    void refreshFromEngine();
    void applyTableToEngine();
    void addRuleRow(const QString& name, const QString& field, const QString& pattern,
        double weight, int minSeverity, bool enabled);
    void removeSelectedRows();

    MetricsEngine* m_engine = nullptr;
    QTableWidget* m_table = nullptr;
};
