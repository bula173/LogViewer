#include "ActorDiscoverer.hpp"

#include "Logger.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace analyzer
{

// Keyword tables
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
static const std::vector<std::string> kOutgoingWords = {
    "out", "outgoing", "send", "sent", "tx", "transmit", "request", "publish"
};
static const std::vector<std::string> kIncomingWords = {
    "in", "incoming", "recv", "received", "rx", "receive", "response", "subscribe"
};

// Scoring helpers
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

int ActorDiscoverer::ScoreDirection(const std::string& fieldName,
                                     const std::set<std::string>& values,
                                     std::set<std::string>& outValues,
                                     std::set<std::string>& inValues)
{
    outValues.clear();
    inValues.clear();
    int score = 0;
    for (const auto& v : values)
    {
        const std::string lower = ToLower(v);
        if (ScoreAgainst(lower, kOutgoingWords) > 0) { outValues.insert(v); score += 5; }
        if (ScoreAgainst(lower, kIncomingWords) > 0) { inValues.insert(v); score += 5; }
    }
    score += ScoreAgainst(ToLower(fieldName), {"direction", "dir", "flow", "mode"});
    return score;
}

// Main discovery
ActorDiscoveryResult ActorDiscoverer::Discover(db::EventsContainer& events, size_t sampleLimit)
{
    const size_t total = events.Size();
    if (total == 0) return {};

    const size_t step = (total + sampleLimit - 1) / sampleLimit;
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

    struct FieldScore {
        std::string field;
        int senderScore, receiverScore, actorScore, labelScore, directionScore;
        int cardinality;
        std::set<std::string>* values;
    };

    std::vector<FieldScore> scored;
    for (auto& [field, vals] : fieldVals)
    {
        const int card = static_cast<int>(vals.size());
        if (card < 2 || card > 200) continue;
        const int senderScore  = ScoreSender(field);
        const int receiverScore = ScoreReceiver(field);
        const int actorScore   = ScoreActor(field);
        const int labelScore   = ScoreLabel(field);
        const int cardBonus = (card <= 30) ? 5 : 0;
        std::set<std::string> out, in;
        const int dirScore = ScoreDirection(field, vals, out, in);
        scored.push_back({field,
                         senderScore > 0 ? senderScore + cardBonus : senderScore,
                         receiverScore > 0 ? receiverScore + cardBonus : receiverScore,
                         actorScore > 0 ? actorScore + cardBonus : actorScore,
                         labelScore > 0 ? labelScore + cardBonus : labelScore,
                         dirScore, card, &vals});
    }

    if (scored.empty()) return {};

    ActorDiscoveryResult result;

    // Mode 1: Pair (from + to)
    {
        FieldScore* senderFs = nullptr, *receiverFs = nullptr;
        for (auto& fs : scored)
        {
            if (!senderFs || fs.senderScore > senderFs->senderScore) senderFs = &fs;
            if (!receiverFs || fs.receiverScore > receiverFs->receiverScore) receiverFs = &fs;
        }

        if (senderFs && receiverFs && senderFs->field != receiverFs->field &&
            senderFs->senderScore > 0 && receiverFs->receiverScore > 0)
        {
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
                pat.mode = ExchangeMode::Pair;
                pat.senderField = senderFs->field;
                pat.receiverField = receiverFs->field;
                pat.confidence = senderFs->senderScore + receiverFs->receiverScore;

                FieldScore* labelFs = nullptr;
                for (auto& fs : scored)
                    if (!labelFs || fs.labelScore > labelFs->labelScore) labelFs = &fs;
                if (labelFs && labelFs->labelScore > 0)
                    pat.labelField = labelFs->field;

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
                pat.actors.erase("");
                pat.description = pat.senderField + " → " + pat.receiverField;
                result.patterns.push_back(std::move(pat));
            }
        }
    }

    // Mode 2: DirectionField
    {
        FieldScore* actorFs = nullptr, *dirFs = nullptr;
        for (auto& fs : scored)
        {
            if (!actorFs || fs.actorScore > actorFs->actorScore) actorFs = &fs;
            if (!dirFs || fs.directionScore > dirFs->directionScore) dirFs = &fs;
        }

        if (actorFs && dirFs && actorFs->field != dirFs->field &&
            actorFs->actorScore > 0 && dirFs->directionScore > 5)
        {
            ExchangePattern pat;
            pat.mode = ExchangeMode::DirectionField;
            pat.actorField = actorFs->field;
            pat.directionField = dirFs->field;
            pat.confidence = actorFs->actorScore + dirFs->directionScore;

            ScoreDirection(dirFs->field, *dirFs->values, pat.outgoingValues, pat.incomingValues);

            for (size_t i = 0; i < total; ++i)
            {
                try {
                    const auto& ev = events.GetEvent(i);
                    const auto a = ev.findByKey(pat.actorField);
                    if (!a.empty()) pat.actors.insert(a);
                } catch (const std::out_of_range&) { break; }
            }
            pat.actors.erase("");
            pat.description = pat.actorField + " + " + pat.directionField;
            result.patterns.push_back(std::move(pat));
        }
    }

    // Modes 3 & 4: SenderOnly / ReceiverOnly
    {
        FieldScore* senderFs = nullptr, *receiverFs = nullptr;
        for (auto& fs : scored)
        {
            if (!senderFs || fs.senderScore > senderFs->senderScore) senderFs = &fs;
            if (!receiverFs || fs.receiverScore > receiverFs->receiverScore) receiverFs = &fs;
        }

        if (senderFs && senderFs->senderScore > 0 && (!receiverFs || receiverFs->receiverScore <= 0))
        {
            bool hasPair = false;
            for (const auto& p : result.patterns)
                if (p.mode == ExchangeMode::Pair && p.senderField == senderFs->field)
                    hasPair = true;
            if (!hasPair)
            {
                ExchangePattern pat;
                pat.mode = ExchangeMode::SenderOnly;
                pat.senderField = senderFs->field;
                pat.confidence = senderFs->senderScore;
                for (size_t i = 0; i < total; ++i)
                {
                    try {
                        const auto& ev = events.GetEvent(i);
                        const auto s = ev.findByKey(pat.senderField);
                        if (!s.empty()) pat.actors.insert(s);
                    } catch (const std::out_of_range&) { break; }
                }
                pat.actors.erase("");
                pat.description = pat.senderField + " → (self)";
                result.patterns.push_back(std::move(pat));
            }
        }

        if (receiverFs && receiverFs->receiverScore > 0 && (!senderFs || senderFs->senderScore <= 0))
        {
            bool hasPair = false;
            for (const auto& p : result.patterns)
                if (p.mode == ExchangeMode::Pair && p.receiverField == receiverFs->field)
                    hasPair = true;
            if (!hasPair)
            {
                ExchangePattern pat;
                pat.mode = ExchangeMode::ReceiverOnly;
                pat.receiverField = receiverFs->field;
                pat.confidence = receiverFs->receiverScore;
                for (size_t i = 0; i < total; ++i)
                {
                    try {
                        const auto& ev = events.GetEvent(i);
                        const auto r = ev.findByKey(pat.receiverField);
                        if (!r.empty()) pat.actors.insert(r);
                    } catch (const std::out_of_range&) { break; }
                }
                pat.actors.erase("");
                pat.description = "(self) → " + pat.receiverField;
                result.patterns.push_back(std::move(pat));
            }
        }
    }

    // Sort by confidence
    std::sort(result.patterns.begin(), result.patterns.end(),
              [](const ExchangePattern& a, const ExchangePattern& b) {
                  return a.confidence > b.confidence;
              });

    // Remaining actor fields
    std::set<std::string> usedFields;
    for (const auto& p : result.patterns)
    {
        usedFields.insert(p.senderField);
        usedFields.insert(p.receiverField);
        usedFields.insert(p.actorField);
    }
    for (const auto& fs : scored)
    {
        if (usedFields.count(fs.field)) continue;
        if (fs.actorScore > 0 || fs.senderScore > 0 || fs.receiverScore > 0)
            result.actorFields.push_back(fs.field);
    }

    return result;
}

} // namespace analyzer
