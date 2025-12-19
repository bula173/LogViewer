#include "AIChatPanel.hpp"
#include "OllamaClient.hpp"
#include "Config.hpp"
#include "Logger.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QScrollBar>
#include <QTime>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>

namespace ui::qt
{

AIChatPanel::AIChatPanel(std::shared_ptr<ai::AIServiceWrapper> aiService,
                         db::EventsContainer& events,
                         QWidget* parent)
    : QWidget(parent)
    , m_aiService(aiService)
    , m_events(events)
{
    BuildUi();
}

void AIChatPanel::BuildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // Chat history display
    m_chatHistory = new QTextEdit(this);
    m_chatHistory->setReadOnly(true);
    m_chatHistory->setPlaceholderText(
        tr("Ask questions about your logs here.\n\n"
           "Examples:\n"
           "  - What errors occurred?\n"
           "  - Summarize the last 50 events\n"
           "  - Are there any patterns in the logs?\n"
           "  - What happened around 14:30?"));
    mainLayout->addWidget(m_chatHistory, 1);

    // Input area
    auto* inputLayout = new QHBoxLayout();
    
    // Context size selector
    inputLayout->addWidget(new QLabel(tr("Context:"), this));
    m_contextSizeSpinBox = new QSpinBox(this);
    m_contextSizeSpinBox->setMinimum(1);
    m_contextSizeSpinBox->setMaximum(10000);
    m_contextSizeSpinBox->setValue(50);
    m_contextSizeSpinBox->setSuffix(tr(" events"));
    m_contextSizeSpinBox->setToolTip(tr("Number of recent log events to include as context"));
    connect(m_contextSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AIChatPanel::OnContextSizeChanged);
    inputLayout->addWidget(m_contextSizeSpinBox);

    // Message input
    m_messageInput = new QLineEdit(this);
    m_messageInput->setPlaceholderText(tr("Type your question about the logs..."));
    connect(m_messageInput, &QLineEdit::returnPressed, this, &AIChatPanel::OnSendClicked);
    inputLayout->addWidget(m_messageInput, 1);

    // Send button
    m_sendButton = new QPushButton(tr("Send"), this);
    m_sendButton->setToolTip(tr("Send message (Enter)"));
    connect(m_sendButton, &QPushButton::clicked, this, &AIChatPanel::OnSendClicked);
    inputLayout->addWidget(m_sendButton);

    // Clear button
    m_clearButton = new QPushButton(tr("Clear"), this);
    m_clearButton->setToolTip(tr("Clear chat history"));
    connect(m_clearButton, &QPushButton::clicked, this, &AIChatPanel::OnClearClicked);
    inputLayout->addWidget(m_clearButton);

    mainLayout->addLayout(inputLayout);

    // Initial context size
    m_maxContextEvents = 50;
}

void AIChatPanel::OnSendClicked()
{
    const QString message = m_messageInput->text().trimmed();
    if (message.isEmpty())
        return;

    m_messageInput->clear();
    SendMessage(message);
}

void AIChatPanel::OnClearClicked()
{
    m_chatHistory->clear();
}

void AIChatPanel::OnContextSizeChanged(int value)
{
    m_maxContextEvents = value;
    util::Logger::Info("AI chat context size changed to: {}", m_maxContextEvents);
}

void AIChatPanel::SendMessage(const QString& message)
{
    if (!m_aiService)
    {
        AppendMessage(tr("System"), tr("AI service not available"), "#FF0000");
        return;
    }

    // Display user message
    AppendMessage(tr("You"), message, "#0066CC");
    
    // Show "thinking" indicator
    AppendMessage(tr("AI"), tr("Thinking..."), "#888888");
    
    m_sendButton->setEnabled(false);
    m_messageInput->setEnabled(false);

    emit MessageSent(message);

    // Run AI request in background thread to avoid blocking UI
    auto future = QtConcurrent::run([this, message]() -> std::string {
        try
        {
            // Format recent events as context
            const std::string logsContext = FormatEventsForContext(static_cast<size_t>(m_maxContextEvents));
            
            // Build prompt with log context and user question
            std::string prompt = "You are analyzing application logs. Here are the most recent log events:\\n\\n";
            prompt += logsContext;
            prompt += "\\n\\nUser question: " + message.toStdString();
            prompt += "\\n\\nProvide a helpful, concise answer based on the logs above:";

            util::Logger::Debug("Sending chat message with {} events context", m_maxContextEvents);

            // Send to AI service
            util::Logger::Debug("Model used: {}", 
                m_aiService->service->GetModelName().empty() ? "default" : m_aiService->service->GetModelName());
            util::Logger::Debug("AI Chat Prompt: {}", prompt);
            return m_aiService->service->SendPrompt(prompt);
        }
        catch (const std::exception& e)
        {
            util::Logger::Error("AI chat error: {}", e.what());
            return "Error: " + std::string(e.what());
        }
    });
    
    // Watch for completion
    auto* watcher = new QFutureWatcher<std::string>(this);
    connect(watcher, &QFutureWatcher<std::string>::finished, this, [this, watcher]() {
        const std::string response = watcher->result();
        
        // Remove "Thinking..." message (last message in history)
        QString html = m_chatHistory->toHtml();
        qsizetype lastPIndex = html.lastIndexOf("<p");
        if (lastPIndex != -1)
        {
            html = html.left(lastPIndex);
            m_chatHistory->setHtml(html);
        }
        
        // Display AI response
        AppendMessage(tr("AI"), QString::fromStdString(response), "#009900");
        
        emit ResponseReceived(QString::fromStdString(response));
        
        m_sendButton->setEnabled(true);
        m_messageInput->setEnabled(true);
        m_messageInput->setFocus();
        
        watcher->deleteLater();
    });
    
    watcher->setFuture(future);
}

void AIChatPanel::AppendMessage(const QString& sender, const QString& message, const QString& color)
{
    // Format message with color and timestamp
    const QString timestamp = QTime::currentTime().toString("hh:mm:ss");
    QString html = QString("<p><span style='color: gray;'>[%1]</span> "
                          "<span style='color: %2; font-weight: bold;'>%3:</span> %4</p>")
                      .arg(timestamp)
                      .arg(color)
                      .arg(sender)
                      .arg(message.toHtmlEscaped().replace("\n", "<br>"));
    
    m_chatHistory->append(html);
    
    // Auto-scroll to bottom
    QScrollBar* scrollBar = m_chatHistory->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

std::string AIChatPanel::FormatEventsForContext(size_t maxEvents) const
{
    std::string formatted;
    const size_t totalEvents = m_events.Size();
    
    if (totalEvents == 0)
    {
        return "No log events available.";
    }
    
    const size_t startIdx = totalEvents > maxEvents ? totalEvents - maxEvents : 0;
    const size_t count = totalEvents - startIdx;
    
    formatted += "Total events: " + std::to_string(totalEvents) + "\n";
    formatted += "Showing last " + std::to_string(count) + " events:\n\n";
    
    for (size_t i = startIdx; i < totalEvents; ++i)
    {
        const auto& event = m_events.GetEvent(static_cast<int>(i));
        const auto& config = config::GetConfig();
        formatted += "[" + std::to_string(i + 1) + "] ";
        formatted += event.findByKey("timestamp") + " ";
        formatted += event.findByKey(config.typeFilterField) + ": ";
        formatted += event.findByKey("message") + "\n";
    }
    
    return formatted;
}

} // namespace ui::qt
