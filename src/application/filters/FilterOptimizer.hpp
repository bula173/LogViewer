#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>

namespace filters
{

/**
 * @brief Performance tracking and optimization for filter operations.
 *
 * Tracks:
 * - Filter application time
 * - Memory usage
 * - Cache hit rates
 * - Provides optimization recommendations
 */
class FilterOptimizer
{
  public:
    struct FilterStats
    {
        std::string filterName;
        int applicationsCount = 0;
        std::chrono::milliseconds totalTime{0};
        size_t averageInputSize = 0;
        size_t averageOutputSize = 0;
        float selectivityRatio = 0.0f;  // output/input ratio
    };

    static FilterOptimizer& getInstance();

    /// Start timing a filter application
    void StartFilterTiming(const std::string& filterName);

    /// End timing and record stats
    void EndFilterTiming(const std::string& filterName,
        size_t inputSize, size_t outputSize);

    /// Get statistics for a specific filter
    const FilterStats* GetFilterStats(const std::string& filterName) const;

    /// Get all filter statistics
    const std::unordered_map<std::string, FilterStats>& GetAllStats() const;

    /// Clear all statistics
    void ClearStats();

    /// Get optimization recommendations
    std::vector<std::string> GetOptimizationTips() const;

    /// Enable/disable profiling (can be disabled in release builds for performance)
    void SetProfilingEnabled(bool enabled) { m_profilingEnabled = enabled; }
    bool IsProfilingEnabled() const { return m_profilingEnabled; }

  private:
    FilterOptimizer() = default;

    std::unordered_map<std::string, FilterStats> m_stats;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> m_startTimes;
    bool m_profilingEnabled = true;
};

}  // namespace filters
