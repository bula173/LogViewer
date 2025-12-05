#pragma once

#include "ai/IAIService.hpp"
#include "db/EventsContainer.hpp"
#include <string>
#include <memory>
#include <vector>

namespace ai
{

/**
 * @brief Analyzes logs using AI to provide insights
 */
class LogAnalyzer
{
public:
    enum class AnalysisType
    {
        Summary,           // Generate executive summary
        ErrorAnalysis,     // Focus on errors and their causes
        PatternDetection,  // Find unusual patterns
        RootCause,        // Identify root cause of issues
        Timeline          // Create timeline narrative
    };

    explicit LogAnalyzer(std::shared_ptr<IAIService> aiService,
                        db::EventsContainer& events);

    /**
     * @brief Perform analysis on current log data
     * @param type Type of analysis to perform
     * @param maxEvents Maximum number of events to include (0 = all)
     * @return Analysis results as text
     */
    std::string Analyze(AnalysisType type, size_t maxEvents = 100);

    /**
     * @brief Check if AI service is ready
     */
    bool IsReady() const;

    /**
     * @brief Get human-readable name for analysis type
     */
    static std::string GetAnalysisTypeName(AnalysisType type);

private:
    std::shared_ptr<IAIService> m_aiService;
    db::EventsContainer& m_events;

    std::string FormatEventsForAI(size_t maxEvents) const;
    std::string BuildPrompt(AnalysisType type, const std::string& logData) const;
};

} // namespace ai
