#pragma once

#include "ai/LogAnalyzer.hpp"
#include "ai/IAIService.hpp"
#include <QWidget>
#include <memory>

class QPushButton;
class QTextEdit;
class QComboBox;
class QSpinBox;
class QLabel;
class QCheckBox;

namespace ui::qt
{

/**
 * @brief Panel for AI-powered log analysis
 */
class AIAnalysisPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AIAnalysisPanel(std::shared_ptr<ai::IAIService> aiService,
                            std::shared_ptr<ai::LogAnalyzer> analyzer,
                            QWidget* parent = nullptr);

signals:
    void AnalysisStarted();
    void AnalysisCompleted(const QString& result);

private slots:
    void OnAnalyzeClicked();
    void OnSetupClicked();
    void OnModelChanged(int index);
    void OnShowInstalledModels();
    void OnRefreshModels();
    void OnPredefinedPromptSelected(int index);
    void OnLoadPromptFile();
    void OnSavePromptFile();

private:
    void BuildUi();
    void UpdateStatusLabel();
    void PopulateModelList();
    void LoadPredefinedPrompts();
    QString LoadPromptFromFile(const QString& filePath);

    std::shared_ptr<ai::IAIService> m_aiService;
    std::shared_ptr<ai::LogAnalyzer> m_analyzer;
    
    QComboBox* m_modelCombo{nullptr};
    QComboBox* m_analysisTypeCombo{nullptr};
    QSpinBox* m_maxEventsSpin{nullptr};
    QPushButton* m_analyzeButton{nullptr};
    QTextEdit* m_resultsText{nullptr};
    QLabel* m_statusLabel{nullptr};
    QCheckBox* m_useCustomPromptCheckbox{nullptr};
    QTextEdit* m_customPromptEdit{nullptr};
    QComboBox* m_predefinedPromptCombo{nullptr};
    QPushButton* m_loadPromptButton{nullptr};
    QPushButton* m_savePromptButton{nullptr};
    
    QMap<QString, QString> m_predefinedPrompts;
};

} // namespace ui::qt
