#pragma once

#include "IAIService.hpp"
#include "AIServiceFactory.hpp"
#include "PluginEventsC.h"
#include <nlohmann/json.hpp>
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

    explicit LogAnalyzer(std::shared_ptr<AIServiceWrapper> aiService);

    /**
     * @brief Perform analysis on current log data
     * @param type Type of analysis to perform
     * @param maxEvents Maximum number of events to include (0 = all)
     * @param filteredIndices Optional: specific event indices to analyze (respects user filters)
     * @return Analysis results as text
     */
    std::string Analyze(AnalysisType type, size_t maxEvents = 100,
                       const std::vector<unsigned long>* filteredIndices = nullptr);

    /**
     * @brief Perform analysis with custom user-defined prompt
     * @param customPrompt User-provided analysis prompt
     * @param maxEvents Maximum number of events to include (0 = all)
     * @param filteredIndices Optional: specific event indices to analyze (respects user filters)
     * @return Analysis results as text
     */
    std::string AnalyzeWithCustomPrompt(const std::string& customPrompt, size_t maxEvents = 100,
                                       const std::vector<unsigned long>* filteredIndices = nullptr);

    /**
     * @brief Check if AI service is ready
     */
    bool IsReady() const;

    /**
     * @brief Get human-readable name for analysis type
     */
    static std::string GetAnalysisTypeName(AnalysisType type);

private:
    std::shared_ptr<AIServiceWrapper> m_aiService;

    std::string FormatEventsForAI(size_t maxEvents, const std::vector<unsigned long>* filteredIndices) const;
    void FormatSingleEvent(std::ostringstream& oss, size_t index, const nlohmann::json& eventJson) const;
    std::string BuildPrompt(AnalysisType type, const std::string& logData) const;
};

} // namespace ai
