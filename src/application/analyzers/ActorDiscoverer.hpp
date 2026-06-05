#pragma once

#include "EventsContainer.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace analyzer
{

// ---------------------------------------------------------------------------
// Exchange mode
// ---------------------------------------------------------------------------

/// How actor send/receive relationships are encoded in the log.
enum class ExchangeMode
{
    /// Two distinct fields hold the sender and receiver names.
    /// Covers: from+to, source+dest, sender+receiver, client+server, …
    Pair,

    /// A single actor field plus a direction-value field that distinguishes
    /// send from receive.  The partner is implied (typically the self actor).
    /// Example: component=X, direction=out|in
    DirectionField,

    /// Only a receiver field is present; the sender is always the self actor
    /// (or unspecified when no self actor is configured).
    /// Example: to=B  — the log generator is always the one sending
    ReceiverOnly,

    /// Only a sender field is present; the receiver is always the self actor.
    /// Example: from=A  — the log generator is always the one receiving
    SenderOnly,

    /// Actor names are embedded in a single field using a separator pattern.
    /// Example: "RBC: message", "Train T001: info" — extract prefix before ":"
    PatternField,
};

// ---------------------------------------------------------------------------
// Exchange pattern
// ---------------------------------------------------------------------------

/// Describes how actor-to-actor message flow can be read from the log events.
struct ExchangePattern
{
    ExchangeMode mode {ExchangeMode::Pair};

    // ── Pair / ReceiverOnly / SenderOnly ──────────────────────────────────
    std::string senderField;    ///< Field carrying the sender name (empty in ReceiverOnly)
    std::string receiverField;  ///< Field carrying the receiver name (empty in SenderOnly)
    std::string labelField;     ///< Best field for message labels

    /// All unique actor names seen (union of sender + receiver values).
    std::set<std::string> actors;

    // ── DirectionField ────────────────────────────────────────────────────
    std::string actorField;       ///< Field carrying the actor name
    std::string directionField;   ///< Field whose VALUE indicates direction
    std::set<std::string> outgoingValues; ///< Values that mean "sending"
    std::set<std::string> incomingValues; ///< Values that mean "receiving"

    // ── PatternField ──────────────────────────────────────────────────────
    std::string patternField;     ///< Field containing embedded actor names
    std::string separator;        ///< Separator character/string (e.g., ":")
    std::string extractionPattern; ///< Regex pattern to extract actor (e.g., "([^:]+):")

    // ── Metadata ──────────────────────────────────────────────────────────
    int         confidence {0}; ///< Score used to rank candidates (higher = better)
    std::string description;    ///< Human-readable summary shown in the Discover dialog
};

// ---------------------------------------------------------------------------
// Discovery result
// ---------------------------------------------------------------------------

struct ActorDiscoveryResult
{
    /// All detected exchange patterns, ranked by confidence (best first).
    /// Most logs yield one; some yield two or three from different field combos.
    std::vector<ExchangePattern> patterns;

    /// Actor-like fields that don't belong to any detected exchange pattern.
    std::vector<std::string> actorFields;

    /// Convenience: returns the best pattern or nullptr.
    [[nodiscard]] const ExchangePattern* bestPattern() const
    {
        return patterns.empty() ? nullptr : &patterns.front();
    }
};

// ---------------------------------------------------------------------------
// Discoverer
// ---------------------------------------------------------------------------

/// Scans an EventsContainer and discovers every plausible actor/exchange
/// pattern without hard-coded field names.
///
/// Detected patterns (in decreasing confidence order):
///   Pair          — two fields: senderField + receiverField
///   DirectionField — one actor field + one direction-value field
///   SenderOnly    — only a sender-type field; receiver = self
///   ReceiverOnly  — only a receiver-type field; sender = self
///
/// Works on every log format: JSON microservices, DLT (ecuid/appid),
/// CAN-bus, XML traces, syslog, custom formats.
class ActorDiscoverer
{
public:
    /// Scan up to @p sampleLimit events and return all discovered patterns.
    static ActorDiscoveryResult Discover(db::EventsContainer& events,
                                          size_t sampleLimit = 10'000);

    // ── Exposed for unit tests ─────────────────────────────────────────────
    static std::string ToLower(const std::string& s);
    static int  ScoreSender   (const std::string& fieldName);
    static int  ScoreReceiver (const std::string& fieldName);
    static int  ScoreActor    (const std::string& fieldName);
    static int  ScoreLabel    (const std::string& fieldName);
    /// Returns how well a field's VALUES match known direction keywords.
    /// @p outCount / @p inCount — how many values matched outgoing / incoming.
    static int  ScoreDirection(const std::string& fieldName,
                               const std::set<std::string>& values,
                               std::set<std::string>& outValues,
                               std::set<std::string>& inValues);

    /// Detects if a field contains embedded actor names with a separator.
    /// Returns (separator, extractedActors, confidence) or empty if not detected.
    /// Example: "RBC: message" → separator=":", actors={"RBC"}
    static std::tuple<std::string, std::set<std::string>, int>
    DetectSeparatorPattern(const std::set<std::string>& fieldValues);
};

} // namespace analyzer
