#pragma once

#include "IStatisticsStrategy.hpp"

namespace ui::qt {

/// Fallback strategy for formats without dedicated statistics (XML, CSV, etc.).
/// Returns no format-specific sections; the panel hides the extra section entirely.
class GenericStatisticsStrategy final : public IStatisticsStrategy {
public:
    bool Matches(db::EventsContainer& /*events*/) const override { return true; }
    std::vector<StatsSection> Compute(
        db::EventsContainer& /*events*/,
        const std::vector<unsigned long>& /*indices*/) const override { return {}; }
};

} // namespace ui::qt
