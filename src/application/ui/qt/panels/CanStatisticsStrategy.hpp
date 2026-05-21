#pragma once

#include "IStatisticsStrategy.hpp"

namespace ui::qt {

/// Format-specific statistics for CAN bus logs (ASC files).
/// Computes Rx/Tx/error frame breakdown, unique IDs, frame rate, and signal ranges.
class CanStatisticsStrategy final : public IStatisticsStrategy {
public:
    bool Matches(db::EventsContainer& events) const override;
    std::vector<StatsSection> Compute(
        db::EventsContainer& events,
        const std::vector<unsigned long>& indices) const override;
};

} // namespace ui::qt
