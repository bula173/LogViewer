#pragma once

#include "ActorDiscoverer.hpp"
#include "EventsContainer.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace analyzer
{

/// One arrow of a sequence diagram: a message from one actor to another.
struct SequenceMessage
{
    std::string   from;
    std::string   to;
    std::string   label;
    std::size_t   eventIndex {0}; ///< Index into the EventsContainer
};

/// True for values that stand in for "no actor" in a sender/receiver field
/// (e.g. `destination=internal` on events that are local to their source).
/// Case-insensitive. Such values are not actors and never get a lifeline.
[[nodiscard]] bool IsPlaceholderActor(std::string_view name);

/// Splits a sender/receiver value naming several actors ("c-west,c-east")
/// into its trimmed, non-empty parts.
[[nodiscard]] std::vector<std::string> SplitActorList(std::string_view value);

/// Collects the actor-to-actor messages of a Pair exchange pattern.
///
/// Only events whose sender AND receiver are real actors yield messages:
/// placeholder values ("internal", "none", …) are skipped and multi-actor
/// values are fanned out into one message per receiver/sender. At most
/// @p limit messages are returned, in event order.
[[nodiscard]] std::vector<SequenceMessage> CollectSequenceMessages(
    db::EventsContainer& events, const ExchangePattern& pattern, std::size_t limit);

} // namespace analyzer
