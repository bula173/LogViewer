#pragma once

#include "EventsContainer.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace analyzer
{

/// A detected from→to message-exchange pattern.
struct ExchangePattern
{
    std::string senderField;    ///< Field that identifies the message sender
    std::string receiverField;  ///< Field that identifies the message receiver
    std::string labelField;     ///< Best field for message labels (type/action/message…)
    std::set<std::string> actors; ///< All unique actor names (union of sender+receiver values)
};

struct ActorDiscoveryResult
{
    std::optional<ExchangePattern> exchange;  ///< Set when a from+to pair was found
    std::vector<std::string>       actorFields; ///< Other actor-like fields (no clear pairing)
};

/// Scans an EventsContainer and discovers actor/exchange patterns without
/// any hard-coded field names — purely by keyword scoring and cardinality.
///
/// Works on every log format: JSON microservices (service/target), DLT
/// (ecuid/appid), CAN-bus (channel), XML traces (sender/receiver), etc.
class ActorDiscoverer
{
public:
    /// Scan up to @p sampleLimit events and return discovery results.
    /// The sample is spread evenly across the container so the result
    /// represents the whole log, not just the beginning.
    static ActorDiscoveryResult Discover(const db::EventsContainer& events,
                                          size_t sampleLimit = 10'000);

    // Exposed for unit tests.
    static int ScoreSender  (const std::string& fieldName);
    static int ScoreReceiver(const std::string& fieldName);
    static int ScoreActor   (const std::string& fieldName);
    static int ScoreLabel   (const std::string& fieldName);

private:
    static std::string ToLower(const std::string& s);
};

} // namespace analyzer
