#pragma once

#include "EventsContainer.hpp"
#include "events/EventsTableView.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace ui::qt
{

/// A panel's temporary "drill down" filter (timeline bucket, trace) set on
/// top of whatever filter the rest of the application had set.
///
/// It remembers that previous filter, so Restore() brings it back instead of
/// clearing every filter, and the events the panel showed before, so the
/// panel can keep showing them while its own filter is active.
class DrillDownFilter
{
  public:
    /// Narrows @p view to @p indices. @p shown: the events the panel shows.
    void Apply(EventsTableView& view, const db::EventsContainer& events,
               const std::vector<unsigned long>& indices,
               const std::vector<unsigned long>& shown)
    {
        if (!IsActive(view, events)) // drilling again keeps the first "before"
        {
            const auto* base = view.GetBaseFilteredIndices();
            m_previous = base ? std::optional(*base) : std::nullopt;
            m_shown    = shown;
        }
        m_applied    = indices;
        m_generation = events.Generation();
        m_active     = true;
        view.SetFilteredEvents(indices);
    }

    /// True while the view still shows the last Apply(): nobody else changed
    /// the filter since and the data set is the same.
    [[nodiscard]] bool IsActive(const EventsTableView& view,
                                const db::EventsContainer& events) const
    {
        if (!m_active || events.Generation() != m_generation)
            return false;
        const auto* base = view.GetBaseFilteredIndices();
        return base && *base == m_applied;
    }

    /// The events the panel showed before the drill-down.
    [[nodiscard]] const std::vector<unsigned long>& Shown() const { return m_shown; }

    /// Puts back the filter that was set before Apply() (only while IsActive:
    /// a filter set by someone else since is left alone).
    void Restore(EventsTableView& view, const db::EventsContainer& events)
    {
        const bool active = IsActive(view, events);
        Reset();
        if (!active)
            return;
        if (m_previous)
            view.SetFilteredEvents(*m_previous);
        else
            view.ClearFilter();
    }

    /// Forgets the drill-down (the filter was replaced by someone else).
    void Reset() { m_active = false; }

  private:
    bool                                      m_active {false};
    std::uint64_t                             m_generation {0};
    std::vector<unsigned long>                m_applied;
    std::optional<std::vector<unsigned long>> m_previous;
    std::vector<unsigned long>                m_shown;
};

} // namespace ui::qt
