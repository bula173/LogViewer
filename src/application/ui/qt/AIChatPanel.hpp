#pragma once

#include "ai/LogAnalyzer.hpp"
#include "ai/IAIService.hpp"
#include "ai/AIServiceFactory.hpp"
#include "db/EventsContainer.hpp"
#include <QWidget>
#include <memory>

class QTextEdit;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace ui::qt
{

/**
 * @brief Interactive chat panel for asking questions about logs
 */
class AIChatPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AIChatPanel(std::shared_ptr<ai::AIServiceWrapper> aiService,
                        db::EventsContainer& events,
                        QWidget* parent = nullptr);

signals:
    void MessageSent(const QString& message);
    void ResponseReceived(const QString& response);

private slots:
    void OnSendClicked();
    void OnClearClicked();
    void OnContextSizeChanged(int value);

private:
    void BuildUi();
    void AppendMessage(const QString& sender, const QString& message, const QString& color);
    void SendMessage(const QString& message);
    std::string FormatEventsForContext(size_t maxEvents) const;

    std::shared_ptr<ai::AIServiceWrapper> m_aiService;
    db::EventsContainer& m_events;
    
    QTextEdit* m_chatHistory{nullptr};
    QLineEdit* m_messageInput{nullptr};
    QPushButton* m_sendButton{nullptr};
    QPushButton* m_clearButton{nullptr};
    QSpinBox* m_contextSizeSpinBox{nullptr};
    
    int m_maxContextEvents{50};
};

} // namespace ui::qt
