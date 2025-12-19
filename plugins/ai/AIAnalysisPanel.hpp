#pragma once

#include "LogAnalyzer.hpp"
#include "IAIService.hpp"
#include "AIServiceFactory.hpp"
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

class EventsTableView;
class AIConfigPanel;

/**
 * @brief Panel for AI-powered log analysis
 */
class AIAnalysisPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AIAnalysisPanel(std::shared_ptr<ai::AIServiceWrapper> aiService,
                            std::shared_ptr<ai::LogAnalyzer> analyzer,
                            EventsTableView* eventsView = nullptr,
                            QWidget* parent = nullptr);

    void SetConfigPanel(AIConfigPanel* configPanel);

signals:
    void AnalysisStarted();
    void AnalysisCompleted(const QString& result);

private slots:
    void OnAnalyzeClicked();
    void OnSetupClicked();
    void OnPredefinedPromptSelected(int index);
    void OnLoadPromptFile();
    void OnSavePromptFile();

private:
    void BuildUi();
    void LoadPredefinedPrompts();
    QString LoadPromptFromFile(const QString& filePath);

    std::shared_ptr<ai::AIServiceWrapper> m_aiService;
    std::shared_ptr<ai::LogAnalyzer> m_analyzer;
    EventsTableView* m_eventsView{nullptr};
    AIConfigPanel* m_configPanel{nullptr};
    
    QPushButton* m_analyzeButton{nullptr};
    QTextEdit* m_resultsText{nullptr};
    QCheckBox* m_useCustomPromptCheckbox{nullptr};
    QTextEdit* m_customPromptEdit{nullptr};
    QComboBox* m_predefinedPromptCombo{nullptr};
    QPushButton* m_loadPromptButton{nullptr};
    QPushButton* m_savePromptButton{nullptr};
    
    QMap<QString, QString> m_predefinedPrompts;
};

} // namespace ui::qt
