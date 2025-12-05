#include "ai/LogAnalyzer.hpp"
#include "util/Logger.hpp"
#include <sstream>
#include <algorithm>

namespace ai
{

LogAnalyzer::LogAnalyzer(std::shared_ptr<IAIService> aiService,
                         db::EventsContainer& events)
    : m_aiService(std::move(aiService))
    , m_events(events)
{
}

std::string LogAnalyzer::Analyze(AnalysisType type, size_t maxEvents)
{
    if (!m_aiService || !m_aiService->IsAvailable())
    {
        return "Error: AI service not available. Please ensure Ollama is running.\n\n"
               "Install Ollama: https://ollama.ai\n"
               "Run: ollama pull llama3.2";
    }

    if (m_events.Size() == 0)
    {
        return "No log data available to analyze.";
    }

    util::Logger::Info("Starting {} analysis on {} events",
        GetAnalysisTypeName(type), 
        maxEvents > 0 ? std::min(maxEvents, m_events.Size()) : m_events.Size());

    const std::string logData = FormatEventsForAI(maxEvents);
    const std::string prompt = BuildPrompt(type, logData);

    try
    {
        const std::string result = m_aiService->SendPrompt(prompt);
        util::Logger::Info("Analysis completed successfully");
        return result;
    }
    catch (const std::exception& e)
    {
        const std::string error = std::string("Analysis failed: ") + e.what();
        util::Logger::Error("{}", error);
        return error;
    }
}

std::string LogAnalyzer::AnalyzeWithCustomPrompt(const std::string& customPrompt, size_t maxEvents)
{
    if (!m_aiService || !m_aiService->IsAvailable())
    {
        return "Error: AI service not available. Please ensure Ollama is running.\n\n"
               "Install Ollama: https://ollama.ai\n"
               "Run: ollama pull llama3.2";
    }

    if (m_events.Size() == 0)
    {
        return "No log data available to analyze.";
    }

    util::Logger::Info("Starting custom prompt analysis on {} events",
        maxEvents > 0 ? std::min(maxEvents, m_events.Size()) : m_events.Size());

    const std::string logData = FormatEventsForAI(maxEvents);
    
    // Build prompt with custom user request
    std::ostringstream prompt;
    prompt << "You are a log analysis expert. Analyze the following application logs.\n\n";
    prompt << "User Request: " << customPrompt << "\n\n";
    prompt << "Log Data:\n" << logData << "\n\n";
    prompt << "Provide a detailed analysis addressing the user's request.";

    try
    {
        const std::string result = m_aiService->SendPrompt(prompt.str());
        util::Logger::Info("Custom analysis completed successfully");
        return result;
    }
    catch (const std::exception& e)
    {
        const std::string error = std::string("Analysis failed: ") + e.what();
        util::Logger::Error("{}", error);
        return error;
    }
}

bool LogAnalyzer::IsReady() const
{
    return m_aiService && m_aiService->IsAvailable();
}

std::string LogAnalyzer::GetAnalysisTypeName(AnalysisType type)
{
    switch (type)
    {
        case AnalysisType::Summary: return "Summary";
        case AnalysisType::ErrorAnalysis: return "Error Analysis";
        case AnalysisType::PatternDetection: return "Pattern Detection";
        case AnalysisType::RootCause: return "Root Cause Analysis";
        case AnalysisType::Timeline: return "Timeline";
        default: return "Unknown";
    }
}

std::string LogAnalyzer::FormatEventsForAI(size_t maxEvents) const
{
    std::ostringstream oss;
    
    const size_t totalEvents = m_events.Size();
    const size_t eventsToInclude = (maxEvents > 0) ? 
        std::min(maxEvents, totalEvents) : totalEvents;

    oss << "Total events in log: " << totalEvents << "\n";
    oss << "Showing " << eventsToInclude << " events:\n\n";

    for (size_t i = 0; i < eventsToInclude; ++i)
    {
        const auto& event = m_events.GetEvent(static_cast<int>(i));
        
        oss << "[" << i << "] ID=" << event.getId();
        
        // Include key fields
        const std::string type = event.findByKey("type");
        if (!type.empty())
            oss << " type=" << type;
            
        const std::string level = event.findByKey("level");
        if (!level.empty())
            oss << " level=" << level;
            
        const std::string message = event.findByKey("message");
        if (!message.empty())
            oss << " msg=\"" << message << "\"";
            
        const std::string timestamp = event.findByKey("timestamp");
        if (!timestamp.empty())
            oss << " time=" << timestamp;
        
        oss << "\n";
    }

    return oss.str();
}

std::string LogAnalyzer::BuildPrompt(AnalysisType type, 
                                     const std::string& logData) const
{
    std::ostringstream prompt;
    
    prompt << "You are a log analysis expert. Analyze the following application logs.\n\n";
    
    switch (type)
    {
        case AnalysisType::Summary:
            prompt << "Task: Provide a concise executive summary of the logs.\n"
                   << "Include:\n"
                   << "- Overall system state (healthy/issues)\n"
                   << "- Key events and their significance\n"
                   << "- Notable patterns or trends\n"
                   << "- Count of errors/warnings if present\n\n";
            break;
            
        case AnalysisType::ErrorAnalysis:
            prompt << "Task: Focus on errors and problems in the logs.\n"
                   << "Include:\n"
                   << "- All errors found and their severity\n"
                   << "- Patterns in error occurrences\n"
                   << "- Potential impact of each error\n"
                   << "- Recommended actions\n\n";
            break;
            
        case AnalysisType::PatternDetection:
            prompt << "Task: Identify unusual or interesting patterns.\n"
                   << "Include:\n"
                   << "- Repetitive sequences\n"
                   << "- Anomalies or outliers\n"
                   << "- Correlation between events\n"
                   << "- Potential issues indicated by patterns\n\n";
            break;
            
        case AnalysisType::RootCause:
            prompt << "Task: Perform root cause analysis.\n"
                   << "Include:\n"
                   << "- Identify the primary issue or failure\n"
                   << "- Trace back to the root cause\n"
                   << "- Explain the chain of events leading to the problem\n"
                   << "- Suggest fixes to prevent recurrence\n\n";
            break;
            
        case AnalysisType::Timeline:
            prompt << "Task: Create a narrative timeline of events.\n"
                   << "Include:\n"
                   << "- Chronological story of what happened\n"
                   << "- Key milestones and transitions\n"
                   << "- Relationships between events\n"
                   << "- Overall flow and progression\n\n";
            break;
    }
    
    prompt << "LOG DATA:\n" << logData << "\n\n";
    prompt << "Analysis:";
    
    return prompt.str();
}

} // namespace ai
