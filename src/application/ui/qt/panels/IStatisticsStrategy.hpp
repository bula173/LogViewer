#pragma once

#include <string>
#include <vector>

namespace db { class EventsContainer; }

namespace ui::qt {

struct StatRow {
    std::string label;
    std::string value;
};

struct StatsSection {
    std::string title;
    std::vector<StatRow> rows;
};

class IStatisticsStrategy {
public:
    virtual ~IStatisticsStrategy() = default;

    /// Returns true if this strategy is applicable to the current event data.
    virtual bool Matches(db::EventsContainer& events) const = 0;

    /// Compute format-specific statistics for the given visible event indices.
    virtual std::vector<StatsSection> Compute(
        db::EventsContainer& events,
        const std::vector<unsigned long>& indices) const = 0;
};

} // namespace ui::qt
