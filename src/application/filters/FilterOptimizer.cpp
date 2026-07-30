#include "FilterOptimizer.hpp"
#include <algorithm>
#include <sstream>

namespace filters
{

FilterOptimizer& FilterOptimizer::getInstance()
{
    static FilterOptimizer instance;
    return instance;
}

void FilterOptimizer::StartFilterTiming(const std::string& filterName)
{
    if (!m_profilingEnabled)
        return;

    m_startTimes[filterName] = std::chrono::high_resolution_clock::now();
}

void FilterOptimizer::EndFilterTiming(const std::string& filterName,
    size_t inputSize, size_t outputSize)
{
    if (!m_profilingEnabled)
        return;

    auto it = m_startTimes.find(filterName);
    if (it == m_startTimes.end())
        return;

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - it->second);

    m_startTimes.erase(it);

    auto& stats = m_stats[filterName];
    stats.filterName = filterName;
    stats.applicationsCount++;

    // Update total time
    stats.totalTime += duration;

    // Update average sizes
    const float alpha = 0.2f;  // exponential moving average factor
    if (stats.applicationsCount == 1) {
        stats.averageInputSize = inputSize;
        stats.averageOutputSize = outputSize;
    } else {
        stats.averageInputSize = static_cast<size_t>(
            alpha * static_cast<float>(inputSize) + (1.0f - alpha) * static_cast<float>(stats.averageInputSize));
        stats.averageOutputSize = static_cast<size_t>(
            alpha * static_cast<float>(outputSize) + (1.0f - alpha) * static_cast<float>(stats.averageOutputSize));
    }

    // Calculate selectivity ratio (output/input)
    stats.selectivityRatio = inputSize > 0 ?
        static_cast<float>(outputSize) / static_cast<float>(inputSize) : 0.0f;
}

const FilterOptimizer::FilterStats* FilterOptimizer::GetFilterStats(
    const std::string& filterName) const
{
    auto it = m_stats.find(filterName);
    if (it != m_stats.end())
        return &it->second;
    return nullptr;
}

const std::unordered_map<std::string, FilterOptimizer::FilterStats>&
FilterOptimizer::GetAllStats() const
{
    return m_stats;
}

void FilterOptimizer::ClearStats()
{
    m_stats.clear();
    m_startTimes.clear();
}

std::vector<std::string> FilterOptimizer::GetOptimizationTips() const
{
    std::vector<std::string> tips;

    if (m_stats.empty())
        return tips;

    // Analyze filter performance patterns
    for (const auto& [name, stats] : m_stats) {
        if (stats.applicationsCount < 2)
            continue;

        auto avgTimeMs = stats.totalTime.count() / stats.applicationsCount;

        // Tip 1: Very selective filters (matching <10% of events)
        if (stats.selectivityRatio < 0.1f) {
            tips.push_back(std::string("Filter '") + name +
                "' is very selective (matches " +
                std::to_string(static_cast<int>(stats.selectivityRatio * 100)) +
                "% of events). Apply it early to reduce dataset.");
        }

        // Tip 2: Slow filters (>100ms per application on large datasets)
        if (avgTimeMs > 100 && stats.averageInputSize > 100000) {
            tips.push_back(std::string("Filter '") + name +
                "' is slow (" + std::to_string(avgTimeMs) + "ms). " +
                "Consider using regex or optimizing the pattern.");
        }

        // Tip 3: Non-selective filters (matching >90% of events)
        if (stats.selectivityRatio > 0.9f) {
            tips.push_back(std::string("Filter '") + name +
                "' is non-selective. Consider combining with other filters.");
        }
    }

    return tips;
}

}  // namespace filters
