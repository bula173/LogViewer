#pragma once

#include "ai/LogAnalyzer.hpp"
#include "ai/IAIService.hpp"
#include "ai/AIServiceFactory.hpp"
#include <QWidget>
#include <memory>

class QComboBox;
class QSpinBox;
class QLabel;
class QPushButton;

namespace ui::qt
{

/**
 * @brief Configuration panel for AI settings
 */
class AIConfigPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AIConfigPanel(std::shared_ptr<ai::AIServiceWrapper> aiService,
                          std::shared_ptr<ai::LogAnalyzer> analyzer,
                          QWidget* parent = nullptr);

    ai::LogAnalyzer::AnalysisType GetAnalysisType() const;
    int GetMaxEvents() const;
    std::shared_ptr<ai::AIServiceWrapper> GetAIService() const { return m_aiService; }
    std::shared_ptr<ai::LogAnalyzer> GetAnalyzer() const { return m_analyzer; }
    
    void RefreshAIClient();

signals:
    void ConfigurationChanged();

private slots:
    void OnProviderChanged(int index);
    void OnModelChanged(int index);
    void OnRefreshModels();
    void OnShowInstalledModels();

private:
    void BuildUi();
    void UpdateStatusLabel();
    void PopulateModelList();

    std::shared_ptr<ai::AIServiceWrapper> m_aiService;
    std::shared_ptr<ai::LogAnalyzer> m_analyzer;
    
    QComboBox* m_providerCombo{nullptr};
    QComboBox* m_modelCombo{nullptr};
    QComboBox* m_analysisTypeCombo{nullptr};
    QSpinBox* m_maxEventsSpin{nullptr};
    QLabel* m_statusLabel{nullptr};
};

} // namespace ui::qt
