#include "ActorDiscoverer.hpp"

#include "Logger.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace analyzer
{

// ---------------------------------------------------------------------------
// Keyword tables
// ---------------------------------------------------------------------------

static const std::vector<std::string> kSenderWords   = {
    "from", "sender", "source", "src", "origin", "initiator", "caller",
    "producer", "publisher", "client", "ecuid"
};
static const std::vector<std::string> kReceiverWords = {
    "to", "dest", "destination", "target", "receiver", "recipient",
    "callee", "consumer", "subscriber", "server", "appid"
};
static const std::vector<std::string> kActorWords    = {
    "actor", "component", "service", "module", "node", "host",
    "process", "proc", "peer", "agent", "role", "channel",
    "endpoint", "worker", "thread"
};
static const std::vector<std::string> kLabelWords    = {
    "type", "action", "message", "msg", "command", "cmd",
    "request", "response", "event", "method", "operation", "kind"
};

// ---------------------------------------------------------------------------
// Scoring helpers
// ---------------------------------------------------------------------------

std::string ActorDiscoverer::ToLower(const std::string& s)
{
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return r;
}

static int ScoreAgainst(const std::string& lower,
                        const std::vector<std::string>& words)
{
    for (const auto& w : words)
        if (lower == w || lower.find(w) != std::string::npos)
            return 10;
    return 0;
}

int ActorDiscoverer::ScoreSender  (const std::string& f) { return ScoreAgainst(ToLower(f), kSenderWords);   }
int ActorDiscoverer::ScoreReceiver(const std::string& f) { return ScoreAgainst(ToLower(f), kReceiverWords); }
int ActorDiscoverer::ScoreActor   (const std::string& f) { return ScoreAgainst(ToLower(f), kActorWords);    }
int ActorDiscoverer::ScoreLabel   (const std::string& f) { return ScoreAgainst(ToLower(f), kLabelWords);    }

// ---------------------------------------------------------------------------
// Main discovery
// ---------------------------------------------------------------------------

ActorDiscoveryResult ActorDiscoverer::Discover(db::EventsContainer& events,
                                                size_t sampleLimit)
{
    const size_t total = events.Size();
    if (total == 0) return {};

    // Ceiling division so step >= 2 whenever total > sampleLimit.
    const size_t step = (total + sampleLimit - 1) / sampleLimit;

    // field → set of distinct values (capped at 201 to detect overflow)
    std::map<std::string, std::set<std::string>> fieldVals;
    for (size_t i = 0; i < total; i += step)
    {
        try {
            for (const auto& [k, v] : events.GetEvent(i).getEventItems())
            {
                auto& s = fieldVals[k];
                if (s.size() < 201) s.insert(v);
            }
        } catch (const std::out_of_range&) { break; }
    }

    struct FieldScore
    {
        std::string field;
        int senderScore;
        int receiverScore;
        int actorScore;
        int labelScore;
        int cardinality;
        std::set<std::string>* values; // pointer into fieldVals
    };

    std::vector<FieldScore> scored;
    scored.reserve(fieldVals.size());

    for (auto& [field, vals] : fieldVals)
    {
        const int card = static_cast<int>(vals.size());
        if (card < 2 || card > 200) continue; // timestamps / unique IDs / constants

        // Cardinality bonus: 2-30 values → likely an actor enum
        const int cardBonus = (card <= 30) ? 5 : 0;

        scored.push_back({
            field,
            ScoreSender(field)   + cardBonus,
            ScoreReceiver(field) + cardBonus,
            ScoreActor(field)    + cardBonus,
            ScoreLabel(field)    + cardBonus,
            card,
            &vals
        });
    }

    if (scored.empty()) return {};

    // Single pass to find the best sender, receiver, and label fields.
    FieldScore* senderFs   = nullptr;
    FieldScore* receiverFs = nullptr;
    FieldScore* labelFs    = nullptr;
    for (auto& fs : scored)
    {
        if (!senderFs   || fs.senderScore   > senderFs->senderScore)     senderFs   = &fs;
        if (!receiverFs || fs.receiverScore > receiverFs->receiverScore)  receiverFs = &fs;
        if (!labelFs    || fs.labelScore    > labelFs->labelScore)        labelFs    = &fs;
    }
    if (senderFs   && senderFs->senderScore   == 0) senderFs   = nullptr;
    if (receiverFs && receiverFs->receiverScore == 0) receiverFs = nullptr;
    if (labelFs    && labelFs->labelScore     == 0) labelFs    = nullptr;

    ActorDiscoveryResult result;

    if (senderFs && receiverFs && senderFs->field != receiverFs->field)
    {
        // Verify the pairing makes sense: the two fields must actually
        // co-exist in at least some events (both non-empty in the same event).
        size_t coExist = 0;
        const size_t probe = std::min(total, size_t(2000));
        const size_t probeStep = total > probe ? total / probe : 1;
        for (size_t i = 0; i < total && coExist < 10; i += probeStep)
        {
            try {
                const auto& ev = events.GetEvent(i);
                if (!ev.findByKey(senderFs->field).empty() &&
                    !ev.findByKey(receiverFs->field).empty())
                    ++coExist;
            } catch (const std::out_of_range&) { break; }
        }

        if (coExist >= 3)
        {
            ExchangePattern pat;
            pat.senderField   = senderFs->field;
            pat.receiverField = receiverFs->field;
            pat.labelField    = labelFs ? labelFs->field : "";

            // Full scan of ALL events to collect every actor name — the
            // sampled fieldVals set may have missed actors that only appear
            // outside the sample window (important for sparse actor values).
            for (size_t i = 0; i < total; ++i)
            {
                try {
                    const auto& ev = events.GetEvent(i);
                    const auto s = ev.findByKey(pat.senderField);
                    const auto r = ev.findByKey(pat.receiverField);
                    if (!s.empty()) pat.actors.insert(s);
                    if (!r.empty()) pat.actors.insert(r);
                } catch (const std::out_of_range&) { break; }
            }
            pat.actors.erase(""); // remove blanks

            result.exchange = std::move(pat);

            util::Logger::Info("[ActorDiscoverer] Exchange pattern: {}→{}, label={}, {} actor(s)",
                senderFs->field, receiverFs->field,
                result.exchange->labelField.empty() ? "(none)" : result.exchange->labelField,
                result.exchange->actors.size());
        }
    }

    // Remaining actor-like fields (not used as from/to)
    for (const auto& fs : scored)
    {
        if (result.exchange &&
            (fs.field == result.exchange->senderField ||
             fs.field == result.exchange->receiverField))
            continue;
        if (fs.actorScore > 0 || fs.senderScore > 0 || fs.receiverScore > 0)
            result.actorFields.push_back(fs.field);
    }

    return result;
}

} // namespace analyzer
