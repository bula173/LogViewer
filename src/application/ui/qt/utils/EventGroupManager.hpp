#pragma once

#include <QString>
#include <vector>
#include <map>
#include <memory>

namespace db {
class LogEvent;
}

namespace ui::qt::utils {

/**
 * @brief Event grouping and clustering system for v1.10.0 Phase 10.
 *
 * Features:
 * - Automatic clustering of similar log events
 * - Multiple grouping strategies (by level, by message pattern, by actor)
 * - Configurable similarity threshold
 * - Group statistics (count, first/last timestamp, severity)
 * - Pattern-based deduplication
 * - Expandable groups in UI
 */
class EventGroupManager
{
  public:
    enum class GroupStrategy
    {
        ByLevel,           // Group by log level
        ByMessage,         // Group by exact message match
        ByPattern,         // Group by similar message patterns
        ByActor,           // Group by actor/source
        ByTimeBucket       // Group by time range (5min buckets)
    };

    struct EventGroup
    {
        QString groupId;
        QString groupName;
        GroupStrategy strategy;
        std::vector<int> eventIndices;
        QString firstMessage;
        int64_t firstTimestamp;
        int64_t lastTimestamp;
        QString severity;
        int count {0};
        bool expanded {false};
    };

    static EventGroupManager& getInstance();

    /// Generate event groups using specified strategy
    std::vector<EventGroup> generateGroups(const std::vector<db::LogEvent*>& events,
                                          GroupStrategy strategy);

    /// Get similarity score between two messages (0.0 to 1.0)
    double calculateSimilarity(const QString& msg1, const QString& msg2) const;

    /// Set similarity threshold for pattern matching (0.0 to 1.0)
    void setSimilarityThreshold(double threshold);
    double getSimilarityThreshold() const { return m_similarityThreshold; }

    /// Get available grouping strategies
    static std::vector<QString> getAvailableStrategies();

    /// Flatten groups back to individual events
    std::vector<int> flattenGroups(const std::vector<EventGroup>& groups) const;

  private:
    EventGroupManager();

    std::vector<EventGroup> GroupByLevel(const std::vector<db::LogEvent*>& events);
    std::vector<EventGroup> GroupByMessage(const std::vector<db::LogEvent*>& events);
    std::vector<EventGroup> GroupByPattern(const std::vector<db::LogEvent*>& events);
    std::vector<EventGroup> GroupByActor(const std::vector<db::LogEvent*>& events);
    std::vector<EventGroup> GroupByTimeBucket(const std::vector<db::LogEvent*>& events);

    QString ExtractPattern(const QString& message) const;
    QString NormalizeMessage(const QString& message) const;

    double m_similarityThreshold {0.8};  // 80% match = similar
    int m_timeBucketMinutes {5};
};

} // namespace ui::qt::utils
