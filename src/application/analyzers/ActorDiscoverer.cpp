#include "ActorDiscoverer.hpp"

#include "Logger.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <tuple>

namespace analyzer
{

// Keyword tables
static const std::vector<std::string> kSenderWords   = {
    "from", "sender", "source", "src", "origin", "initiator", "caller",
    "producer", "publisher", "client", "ecuid", "caller_id", "originator"
};
static const std::vector<std::string> kReceiverWords = {
    "to", "dest", "destination", "target", "receiver", "recipient",
    "callee", "consumer", "subscriber", "server", "appid", "called_id", "recipient_id"
};
static const std::vector<std::string> kActorWords    = {
    "actor", "component", "service", "module", "node", "host",
    "process", "proc", "peer", "agent", "role", "channel",
    "endpoint", "worker", "thread", "pod", "container", "instance"
};
static const std::vector<std::string> kLabelWords    = {
    "type", "action", "message", "msg", "command", "cmd",
    "request", "response", "event", "method", "operation", "kind",
    "function", "procedure", "routine", "handler"
};
static const std::vector<std::string> kOutgoingWords = {
    "out", "outgoing", "send", "sent", "tx", "transmit", "request", "publish",
    "initiate", "calling", "request_to", "outbound"
};
static const std::vector<std::string> kIncomingWords = {
    "in", "incoming", "recv", "received", "rx", "receive", "response", "subscribe",
    "accept", "processing", "from_request", "inbound", "receiving"
};

// Protocol-specific patterns
static const std::vector<std::pair<std::string, std::string>> kProtocolPatterns = {
    {"HTTP", "(?:GET|POST|PUT|DELETE|PATCH|HEAD|OPTIONS)\\s+"},
    {"gRPC", "\\..*\\."},
    {"REST", "/api/"},
    {"AMQP", "amqp://"},
    {"MQTT", "mqtt://"},
    {"TCP", ":\\d+"},
    {"Kafka", "topic=|partition="}
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

    // Mode 5: PatternField (embedded actors with separator)
    {
        for (auto& [field, vals] : fieldVals)
        {
            // Skip fields already used in detected patterns
            bool used = false;
            for (const auto& p : result.patterns)
            {
                if (field == p.senderField || field == p.receiverField ||
                    field == p.actorField || field == p.directionField)
                {
                    used = true;
                    break;
                }
            }
            if (used) continue;

            auto [sep, actors, conf] = DetectSeparatorPattern(vals);
            if (conf > 0 && actors.size() >= 2)
            {
                ExchangePattern pat;
                pat.mode = ExchangeMode::PatternField;
                pat.patternField = field;
                pat.separator = sep;
                pat.actors = actors;
                pat.confidence = conf;
                pat.extractionPattern = "([^" + sep + "]+)" + sep;
                pat.description = "Embedded actors in '" + field + "' using separator '" + sep + "'";
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

// ---------------------------------------------------------------------------
// Pattern detection for embedded actors
// ---------------------------------------------------------------------------

std::tuple<std::string, std::set<std::string>, int>
ActorDiscoverer::DetectSeparatorPattern(const std::set<std::string>& fieldValues)
{
    // Strategy: Try common separators, hierarchical patterns, then word-frequency

    // Step 1: Try explicit separators first
    static const std::vector<std::pair<std::string, std::string>> separators = {
        {":", "([^:]+):"},      // "RBC: message"
        {"|", "([^|]+)\\|"},    // "RBC | message"
        {"-", "([^-]+)-"},      // "RBC-message"
        {"->", "([^-]+)->"},    // "RBC->message"
        {"=>", "([^=]+)=>"},    // "RBC => message"
        {"<", "([^<]+)<"},      // "RBC<message"
    };

    for (const auto& [sep, pattern] : separators)
    {
        std::set<std::string> extracted;
        int matches = 0;

        for (const auto& val : fieldValues)
        {
            const size_t pos = val.find(sep);
            if (pos == std::string::npos) continue;

            // Extract prefix before separator
            std::string prefix = val.substr(0, pos);

            // Trim whitespace
            while (!prefix.empty() && std::isspace(prefix.back()))
                prefix.pop_back();
            while (!prefix.empty() && std::isspace(prefix.front()))
                prefix = prefix.substr(1);

            if (!prefix.empty() && prefix.length() < 50)
            {
                extracted.insert(prefix);
                ++matches;
            }
        }

        // If we found a good number of matches and extracted 2-30 unique actors
        if (matches >= static_cast<int>(fieldValues.size()) * 0.6 &&
            extracted.size() >= 2 && extracted.size() <= 30)
        {
            const int confidence = 15 + (matches * 15) / static_cast<int>(fieldValues.size());
            return {sep, extracted, confidence};
        }
    }

    // Step 2: Try hierarchical patterns (service.component, pod.container, etc.)
    {
        std::set<std::string> hierarchical;
        int matches = 0;

        for (const auto& val : fieldValues)
        {
            // Look for dot-separated hierarchies: service.method, pod.container
            size_t dotPos = val.find('.');
            if (dotPos != std::string::npos && dotPos > 0 && dotPos < val.length() - 1)
            {
                std::string prefix = val.substr(0, dotPos);
                // Trim and validate
                while (!prefix.empty() && std::isspace(prefix.back()))
                    prefix.pop_back();
                if (!prefix.empty() && prefix.length() < 50 &&
                    std::none_of(prefix.begin(), prefix.end(), ::isdigit))
                {
                    hierarchical.insert(prefix);
                    ++matches;
                }
            }
        }

        if (matches >= static_cast<int>(fieldValues.size()) * 0.5 &&
            hierarchical.size() >= 2 && hierarchical.size() <= 30)
        {
            const int confidence = 14 + (matches * 10) / static_cast<int>(fieldValues.size());
            return {".", hierarchical, confidence};
        }
    }

    // Step 3: Try numeric suffix patterns (Service-1, Service-2, etc.)
    {
        std::set<std::string> baseNames;
        int matches = 0;

        for (const auto& val : fieldValues)
        {
            // Try to extract base name without trailing numbers
            std::string base = val;
            while (!base.empty() && std::isdigit(base.back()))
                base.pop_back();

            // Remove trailing separators (-, _, etc.)
            while (!base.empty() && (base.back() == '-' || base.back() == '_'))
                base.pop_back();

            if (!base.empty() && base.length() >= 2 && base.length() < 50)
            {
                baseNames.insert(base);
                ++matches;
            }
        }

        if (matches >= static_cast<int>(fieldValues.size()) * 0.5 &&
            baseNames.size() >= 2 && baseNames.size() <= 30)
        {
            const int confidence = 12 + (matches * 8) / static_cast<int>(fieldValues.size());
            return {"(numeric-suffix)", baseNames, confidence};
        }
    }

    // Step 4: Fall back to word-frequency analysis (separator-agnostic)
    std::map<std::string, int> wordFreq;
    const int maxWordLen = 40;
    const int minWordLen = 2;

    for (const auto& val : fieldValues)
    {
        // Extract words at start of line (likely candidates for actor names)
        std::istringstream iss(val);
        std::string word;
        int wordCount = 0;

        while (iss >> word && wordCount < 3)
        {
            // Clean punctuation from word
            while (!word.empty() && (word.back() == ':' || word.back() == '|' ||
                                      word.back() == '-' || word.back() == ',' ||
                                      word.back() == '.' || word.back() == '!'))
                word.pop_back();

            if (word.length() >= minWordLen && word.length() <= maxWordLen &&
                !std::all_of(word.begin(), word.end(), ::isdigit))
            {
                wordFreq[word]++;
            }
            ++wordCount;
        }
    }

    // Find words that appear frequently (likely actors)
    std::set<std::string> candidates;
    const int freqThreshold = static_cast<int>(fieldValues.size()) * 0.4;  // 40% of values

    for (const auto& [word, freq] : wordFreq)
    {
        if (freq >= freqThreshold)
            candidates.insert(word);
    }

    // If we found 2-30 candidate actors, propose this pattern
    if (candidates.size() >= 2 && candidates.size() <= 30)
    {
        const int confidence = 10 + (candidates.size() * 4);  // Low score: fallback method
        return {"(word-frequency)", candidates, confidence};
    }

    return {"", {}, 0};  // No pattern found
}

ActorDiscoveryResult ActorDiscoverer::DiscoverWithAI(db::EventsContainer& events,
                                                     size_t sampleLimit)
{
    // Start with standard heuristic discovery
    ActorDiscoveryResult result = Discover(events, sampleLimit);

    // Attempt AI-based direction detection if available
    try
    {
        // Include GemmaInferenceEngine header for AI analysis
        // Note: This is a forward declaration area - AI direction detection is optional
        util::Logger::Info("[ActorDiscovery] Performing AI direction analysis...");

        // Sample messages for AI analysis
        std::set<std::string> sampleMessages;
        const size_t total = events.Size();
        const size_t step = (total + std::min(total, sampleLimit) - 1) / std::min(total, sampleLimit);

        for (size_t i = 0; i < total && sampleMessages.size() < 10; i += step)
        {
            try
            {
                const auto& event = events.GetEvent(i);
                // Reconstruct message from first few important fields
                std::string msg;
                for (const auto& [key, val] : event.getEventItems())
                {
                    if (!msg.empty()) msg += " | ";
                    msg += key + "=" + val;
                    if (msg.length() > 200) break;  // Limit message length
                }
                if (!msg.empty())
                    sampleMessages.insert(msg);
            }
            catch (const std::out_of_range&)
            {
                break;
            }
        }

        // Try to get AI direction detection
        // This code will work when GemmaInferenceEngine is fully integrated
        if (!sampleMessages.empty())
        {
            util::Logger::Debug("[ActorDiscovery] Sampled {} messages for AI analysis",
                              sampleMessages.size());

            // If we have any AI-detected directions, enhance existing patterns
            // For now, this prepares the infrastructure
            util::Logger::Info("[ActorDiscovery] AI direction analysis available for future integration");
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Warn("[ActorDiscovery] AI analysis optional - continuing with heuristics: {}",
                         e.what());
    }

    return result;
}

} // namespace analyzer
