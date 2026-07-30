#include "EventGroupManager.hpp"

#include "Logger.hpp"
#include "../../../db/EventsContainer.hpp"

#include <QRegularExpression>
#include <algorithm>
#include <unordered_map>
#include <cmath>

namespace ui::qt::utils {

EventGroupManager& EventGroupManager::getInstance()
{
    static EventGroupManager instance;
    return instance;
}

EventGroupManager::EventGroupManager()
{
}

std::vector<EventGroupManager::EventGroup> EventGroupManager::generateGroups(
    const std::vector<db::LogEvent*>& events,
    GroupStrategy strategy)
{
    util::Logger::Debug("[EventGroupManager] Generating groups using strategy {}", static_cast<int>(strategy));

    switch (strategy)
    {
        case GroupStrategy::ByLevel:
            return GroupByLevel(events);
        case GroupStrategy::ByMessage:
            return GroupByMessage(events);
        case GroupStrategy::ByPattern:
            return GroupByPattern(events);
        case GroupStrategy::ByActor:
            return GroupByActor(events);
        case GroupStrategy::ByTimeBucket:
            return GroupByTimeBucket(events);
    }

    return {};
}

double EventGroupManager::calculateSimilarity(const QString& msg1, const QString& msg2) const
{
    if (msg1 == msg2)
        return 1.0;

    QString norm1 = NormalizeMessage(msg1);
    QString norm2 = NormalizeMessage(msg2);

    int maxLen = std::max(norm1.length(), norm2.length());
    if (maxLen == 0)
        return 1.0;

    // Levenshtein distance-based similarity
    std::vector<std::vector<int>> dp(norm1.length() + 1,
        std::vector<int>(norm2.length() + 1));

    for (size_t i = 0; i <= norm1.length(); ++i)
        dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= norm2.length(); ++j)
        dp[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= norm1.length(); ++i)
    {
        for (size_t j = 1; j <= norm2.length(); ++j)
        {
            int cost = (norm1[static_cast<int>(i) - 1] == norm2[static_cast<int>(j) - 1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i - 1][j] + 1,
                dp[i][j - 1] + 1,
                dp[i - 1][j - 1] + cost
            });
        }
    }

    int distance = dp[norm1.length()][norm2.length()];
    double similarity = 1.0 - (static_cast<double>(distance) / maxLen);
    return std::max(0.0, similarity);
}

void EventGroupManager::setSimilarityThreshold(double threshold)
{
    m_similarityThreshold = std::clamp(threshold, 0.0, 1.0);
    util::Logger::Debug("[EventGroupManager] Similarity threshold set to {}", m_similarityThreshold);
}

std::vector<QString> EventGroupManager::getAvailableStrategies()
{
    return {
        "By Level",
        "By Message",
        "By Pattern",
        "By Actor",
        "By Time Bucket"
    };
}

std::vector<int> EventGroupManager::flattenGroups(const std::vector<EventGroup>& groups) const
{
    std::vector<int> result;
    for (const auto& group : groups)
        result.insert(result.end(), group.eventIndices.begin(), group.eventIndices.end());
    return result;
}

std::vector<EventGroupManager::EventGroup> EventGroupManager::GroupByLevel(
    const std::vector<db::LogEvent*>& events)
{
    std::unordered_map<QString, EventGroup, std::hash<QString>> groupMap;
    std::vector<QString> levelOrder = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};

    for (size_t i = 0; i < events.size(); ++i)
    {
        const auto* event = events[i];
        QString level = QString::fromStdString(event->findByKey("level")).toUpper();

        if (groupMap.find(level) == groupMap.end())
        {
            EventGroup group;
            group.groupId = level;
            group.groupName = level;
            group.strategy = GroupStrategy::ByLevel;
            group.severity = level;
            groupMap[level] = group;
        }

        groupMap[level].eventIndices.push_back(static_cast<int>(i));
    }

    // Sort and finalize
    std::vector<EventGroup> result;
    for (const auto& level : levelOrder)
    {
        auto it = groupMap.find(level);
        if (it != groupMap.end())
        {
            it->second.count = static_cast<int>(it->second.eventIndices.size());
            result.push_back(it->second);
        }
    }

    util::Logger::Info("[EventGroupManager] Grouped {} events by level into {} groups",
                      events.size(), result.size());
    return result;
}

std::vector<EventGroupManager::EventGroup> EventGroupManager::GroupByMessage(
    const std::vector<db::LogEvent*>& events)
{
    std::map<QString, EventGroup> groupMap;

    for (size_t i = 0; i < events.size(); ++i)
    {
        const auto* event = events[i];
        QString message = QString::fromStdString(event->findByKey("message"));
        if (message.isEmpty())
            message = QString::fromStdString(event->findByKey("msg"));

        if (groupMap.find(message) == groupMap.end())
        {
            EventGroup group;
            group.groupId = QString::number(groupMap.size());
            group.groupName = message.left(50) + (message.length() > 50 ? "..." : "");
            group.strategy = GroupStrategy::ByMessage;
            group.firstMessage = message;
            groupMap[message] = group;
        }

        groupMap[message].eventIndices.push_back(static_cast<int>(i));
    }

    // Convert to vector
    std::vector<EventGroup> result;
    for (auto& [msg, group] : groupMap)
    {
        group.count = static_cast<int>(group.eventIndices.size());
        result.push_back(std::move(group));
    }

    util::Logger::Info("[EventGroupManager] Grouped {} events by message into {} groups",
                      events.size(), result.size());
    return result;
}

std::vector<EventGroupManager::EventGroup> EventGroupManager::GroupByPattern(
    const std::vector<db::LogEvent*>& events)
{
    std::vector<EventGroup> groups;
    std::vector<bool> assigned(events.size(), false);

    for (size_t i = 0; i < events.size(); ++i)
    {
        if (assigned[i])
            continue;

        const auto* event = events[i];
        QString message = QString::fromStdString(event->findByKey("message"));
        QString pattern = ExtractPattern(message);

        EventGroup group;
        group.groupId = QString::number(groups.size());
        group.groupName = pattern;
        group.strategy = GroupStrategy::ByPattern;
        group.firstMessage = message;

        group.eventIndices.push_back(static_cast<int>(i));
        assigned[i] = true;

        // Find similar messages
        for (size_t j = i + 1; j < events.size(); ++j)
        {
            if (assigned[j])
                continue;

            const auto* otherEvent = events[j];
            QString otherMessage = QString::fromStdString(otherEvent->findByKey("message"));
            double similarity = calculateSimilarity(message, otherMessage);

            if (similarity >= m_similarityThreshold)
            {
                group.eventIndices.push_back(static_cast<int>(j));
                assigned[j] = true;
            }
        }

        group.count = static_cast<int>(group.eventIndices.size());
        groups.push_back(group);
    }

    util::Logger::Info("[EventGroupManager] Grouped {} events by pattern into {} groups",
                      events.size(), groups.size());
    return groups;
}

std::vector<EventGroupManager::EventGroup> EventGroupManager::GroupByActor(
    const std::vector<db::LogEvent*>& events)
{
    std::map<QString, EventGroup> groupMap;

    for (size_t i = 0; i < events.size(); ++i)
    {
        const auto* event = events[i];
        QString actor = QString::fromStdString(event->findByKey("actor"));
        if (actor.isEmpty())
            actor = "Unknown";

        if (groupMap.find(actor) == groupMap.end())
        {
            EventGroup group;
            group.groupId = actor;
            group.groupName = actor;
            group.strategy = GroupStrategy::ByActor;
            groupMap[actor] = group;
        }

        groupMap[actor].eventIndices.push_back(static_cast<int>(i));
    }

    // Convert to vector
    std::vector<EventGroup> result;
    for (auto& [actor, group] : groupMap)
    {
        group.count = static_cast<int>(group.eventIndices.size());
        result.push_back(std::move(group));
    }

    util::Logger::Info("[EventGroupManager] Grouped {} events by actor into {} groups",
                      events.size(), result.size());
    return result;
}

std::vector<EventGroupManager::EventGroup> EventGroupManager::GroupByTimeBucket(
    const std::vector<db::LogEvent*>& events)
{
    std::map<int64_t, EventGroup> groupMap;

    for (size_t i = 0; i < events.size(); ++i)
    {
        const auto* event = events[i];
        // Timestamp would be extracted from event data, using placeholder for now
        // In production, would parse timestamp field to milliseconds
        int64_t timestamp = static_cast<int64_t>(i) * 1000;  // Placeholder
        int64_t bucket = (timestamp / (m_timeBucketMinutes * 60 * 1000)) *
                        (m_timeBucketMinutes * 60 * 1000);

        if (groupMap.find(bucket) == groupMap.end())
        {
            EventGroup group;
            group.groupId = QString::number(bucket);
            group.groupName = QString("Time: %1").arg(bucket);
            group.strategy = GroupStrategy::ByTimeBucket;
            groupMap[bucket] = group;
        }

        groupMap[bucket].eventIndices.push_back(static_cast<int>(i));
    }

    // Convert to vector
    std::vector<EventGroup> result;
    for (auto& [bucket, group] : groupMap)
    {
        group.count = static_cast<int>(group.eventIndices.size());
        result.push_back(std::move(group));
    }

    util::Logger::Info("[EventGroupManager] Grouped {} events by time into {} groups",
                      events.size(), result.size());
    return result;
}

QString EventGroupManager::ExtractPattern(const QString& message) const
{
    // Replace numbers with #, IPs with @, UUIDs with $
    QString pattern = message;
    pattern.replace(QRegularExpression("\\d+"), "#");
    pattern.replace(QRegularExpression("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}"), "@");
    return pattern.left(50);
}

QString EventGroupManager::NormalizeMessage(const QString& message) const
{
    QString normalized = message.toLower();
    // Remove special characters for comparison
    normalized.replace(QRegularExpression("[^a-z0-9]"), "");
    return normalized;
}

} // namespace ui::qt::utils
