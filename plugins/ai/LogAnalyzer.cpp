#include "LogAnalyzer.hpp"
#include "PluginLoggerC.h"
#include <fmt/format.h>
#include <sstream>
#include <algorithm>
#include "PluginEventsC.h"
#include <nlohmann/json.hpp>

// Helper macro for formatted plugin logging
#define PLUGIN_LOG(level, ...) do { std::string _pl_msg = fmt::format(__VA_ARGS__); PluginLogger_Log(level, _pl_msg.c_str()); } while(0)

namespace ai
{

LogAnalyzer::LogAnalyzer(std::shared_ptr<AIServiceWrapper> aiService)
    : m_aiService(std::move(aiService))
{
}

std::string LogAnalyzer::Analyze(AnalysisType type, size_t maxEvents,
                                const std::vector<unsigned long>* filteredIndices)
{
    if (!m_aiService || !m_aiService->service->IsAvailable())
    {
        return "Error: AI service not available. Please setup the AI provider.";
    }

    const int totalEvents = PluginEvents_GetSize();
    if (totalEvents == 0)
    {
        return "No log data available to analyze.";
    }

    const size_t eventCount = filteredIndices ? filteredIndices->size() : static_cast<size_t>(totalEvents);
    PLUGIN_LOG(PLUGIN_LOG_INFO, "Starting {} analysis on {} {} events",
        GetAnalysisTypeName(type),
        maxEvents > 0 ? std::min(maxEvents, eventCount) : eventCount,
        filteredIndices ? "filtered" : "total");

    const std::string logData = FormatEventsForAI(maxEvents, filteredIndices);
    const std::string prompt = BuildPrompt(type, logData);

    try
    {
        const std::string result = m_aiService->service->SendPrompt(prompt);
        PLUGIN_LOG(PLUGIN_LOG_INFO, "Analysis completed successfully");
        return result;
    }
    catch (const std::exception& e)
    {
        const std::string error = std::string("Analysis failed: ") + e.what();
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "{}", error);
        return error;
    }
}

std::string LogAnalyzer::AnalyzeWithCustomPrompt(const std::string& customPrompt, size_t maxEvents,
                                                const std::vector<unsigned long>* filteredIndices)
{
    if (!m_aiService || !m_aiService->service->IsAvailable())
    {
        return "Error: AI service not available. Please setup the AI provider.";
    }

    const int totalEvents = PluginEvents_GetSize();
    if (totalEvents == 0)
    {
        return "No log data available to analyze.";
    }

    const size_t eventCount = filteredIndices ? filteredIndices->size() : static_cast<size_t>(totalEvents);
    PLUGIN_LOG(PLUGIN_LOG_INFO, "Starting custom prompt analysis on {} {} events",
        maxEvents > 0 ? std::min(maxEvents, eventCount) : eventCount,
        filteredIndices ? "filtered" : "total");

    const std::string logData = FormatEventsForAI(maxEvents, filteredIndices);
    
    // Build prompt with custom user request
    std::ostringstream prompt;
    prompt << "You are a log analysis expert. Analyze the following application logs.\n\n";
    prompt << "User Request: " << customPrompt << "\n\n";
    prompt << "Log Data:\n" << logData << "\n\n";
    prompt << "Provide a detailed analysis addressing the user's request.";

    try
    {
        const std::string result = m_aiService->service->SendPrompt(prompt.str());
        PLUGIN_LOG(PLUGIN_LOG_INFO, "Custom analysis completed successfully");
        return result;
    }
    catch (const std::exception& e)
    {
        const std::string error = std::string("Analysis failed: ") + e.what();
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "{}", error);
        return error;
    }
}

bool LogAnalyzer::IsReady() const
{
    return m_aiService && m_aiService->service->IsAvailable();
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

std::string LogAnalyzer::FormatEventsForAI(size_t maxEvents, 
                                          const std::vector<unsigned long>* filteredIndices) const
{
    std::ostringstream oss;
    
    const size_t totalEvents = static_cast<size_t>(PluginEvents_GetSize());
    const size_t availableEvents = filteredIndices ? filteredIndices->size() : totalEvents;
    size_t eventsToInclude = (maxEvents > 0) ? 
        std::min(maxEvents, availableEvents) : availableEvents;

    oss << "Total events in log: " << totalEvents << "\n";
    
    if (filteredIndices)
    {
        oss << "Filtered events: " << filteredIndices->size() << "\n";
        oss << "Note: Analysis respects active filters (type, search, etc.)\n";
    }
    
    // For large datasets, use smart sampling to stay within limits
    if (eventsToInclude > 5000)
    {
        oss << "Using intelligent sampling for large dataset...\n";
        eventsToInclude = 5000;
        oss << "Sampled " << eventsToInclude << " representative events:\n\n";
        
        if (filteredIndices)
        {
            // Sample from filtered indices
            const size_t step = filteredIndices->size() / eventsToInclude;
            for (size_t i = 0; i < eventsToInclude; ++i)
            {
                const size_t filteredIndex = i * step;
                const unsigned long eventIndex = (*filteredIndices)[filteredIndex];
                char* jsonStr = PluginEvents_GetEventJson(static_cast<int>(eventIndex));
                if (jsonStr) {
                    try {
                        nlohmann::json ev = nlohmann::json::parse(jsonStr);
                        FormatSingleEvent(oss, eventIndex, ev);
                    } catch (...) {}
                    std::free(jsonStr);
                }
            }
        }
        else
        {
            // Sample from all events
            const size_t step = totalEvents / eventsToInclude;
            for (size_t i = 0; i < eventsToInclude; ++i)
            {
                const size_t eventIndex = i * step;
                char* jsonStr = PluginEvents_GetEventJson(static_cast<int>(eventIndex));
                if (jsonStr) {
                    try {
                        nlohmann::json ev = nlohmann::json::parse(jsonStr);
                        FormatSingleEvent(oss, eventIndex, ev);
                    } catch (...) {}
                    std::free(jsonStr);
                }
            }
        }
    }
    else
    {
        oss << "Showing " << eventsToInclude << " events:\n\n";
        
        if (filteredIndices)
        {
            // Use only filtered indices
            for (size_t i = 0; i < eventsToInclude; ++i)
            {
                const unsigned long eventIndex = (*filteredIndices)[i];
                char* jsonStr = PluginEvents_GetEventJson(static_cast<int>(eventIndex));
                if (jsonStr) {
                    try {
                        nlohmann::json ev = nlohmann::json::parse(jsonStr);
                        FormatSingleEvent(oss, eventIndex, ev);
                    } catch (...) {}
                    std::free(jsonStr);
                }
            }
        }
        else
        {
            // Send events sequentially from the start
            for (size_t i = 0; i < eventsToInclude; ++i)
            {
                char* jsonStr = PluginEvents_GetEventJson(static_cast<int>(i));
                if (jsonStr) {
                    try {
                        nlohmann::json ev = nlohmann::json::parse(jsonStr);
                        FormatSingleEvent(oss, i, ev);
                    } catch (...) {}
                    std::free(jsonStr);
                }
            }
        }
    }

    return oss.str();
}

void LogAnalyzer::FormatSingleEvent(std::ostringstream& oss, size_t index, const nlohmann::json& eventJson) const
{
    // Use 'id' field if present
    std::string id = "";
    if (eventJson.contains("id")) id = eventJson["id"].get<std::string>();
    oss << "Event[" << index << "] ID=" << id << "\n";

    // Include string fields from the event JSON
    for (auto it = eventJson.begin(); it != eventJson.end(); ++it)
    {
        if (it.value().is_string())
        {
            const std::string key = it.key();
            const std::string value = it.value().get<std::string>();
            if (!value.empty())
                oss << "  " << key << ": " << value << "\n";
        }
    }

    oss << "\n";
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
