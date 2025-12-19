#include "MetricsEngine.hpp"

#include <algorithm>
#include <fstream>
#include <numeric>
#include <sstream>

MetricsEngine::MetricsEngine() = default;

void MetricsEngine::SetEventsContainer(std::shared_ptr<db::EventsContainer> events)
{
    m_events = std::move(events);
}

bool MetricsEngine::LoadConfig(const std::string& path)
{
    try
    {
        boost::property_tree::ptree tree;
        boost::property_tree::read_json(path, tree);
        m_rules.clear();

        if (auto opt = tree.get_child_optional("rules"))
        {
            for (const auto& child : *opt)
            {
                Rule rule;
                rule.name = child.second.get<std::string>("name", "");
                rule.fieldKey = child.second.get<std::string>("field", "message");
                const auto pattern = child.second.get<std::string>("pattern", ".*");
                rule.pattern = boost::regex(pattern, boost::regex::perl | boost::regex::icase);
                rule.weight = child.second.get<double>("weight", 1.0);
                rule.minSeverity = child.second.get<int>("minSeverity", -1);
                rule.enabled = child.second.get<bool>("enabled", true);
                m_rules.push_back(std::move(rule));
            }
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool MetricsEngine::SaveConfig(const std::string& path) const
{
    try
    {
        boost::property_tree::ptree tree;
        boost::property_tree::ptree rules;
        for (const auto& rule : m_rules)
        {
            boost::property_tree::ptree node;
            node.put("name", rule.name);
            node.put("field", rule.fieldKey);
            node.put("pattern", rule.pattern.str());
            node.put("weight", rule.weight);
            node.put("minSeverity", rule.minSeverity);
            node.put("enabled", rule.enabled);
            rules.push_back({"", node});
        }
        tree.add_child("rules", rules);
        boost::property_tree::write_json(path, tree);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::vector<MetricsEngine::Result> MetricsEngine::Analyze() const
{
    std::vector<Result> results;
    if (!m_events)
    {
        return results;
    }

    const size_t ruleCount = m_rules.size();

    // Simple scoring: count regex hits and compute a dummy Mahalanobis-based score.
    // This is a placeholder for a more advanced statistical model.
    const auto total = m_events->Size();
    for (size_t i = 0; i < total; ++i)
    {
        const auto& e = m_events->GetItem(static_cast<int>(i));
        double score = 0.0;
        std::vector<std::string> matchedReasons;
        bool matchedAll = m_rules.empty();
        std::vector<double> perRule(ruleCount, 0.0);

        // Extract message with fallbacks
        std::string message = e.findByKey("message");
        if (message.empty())
        {
            message = e.findByKey("msg");
        }

        std::string timestamp = e.findByKey("timestamp");
        if (timestamp.empty())
        {
            timestamp = e.findByKey("time");
        }

        // Extract severity; accept numeric or textual values; never throw
        auto severityStr = e.findByKey("severity");
        int severityVal = 0;
        if (!severityStr.empty())
        {
            std::string s = severityStr;
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            if (s == "trace") severityVal = 0;
            else if (s == "debug") severityVal = 1;
            else if (s == "info") severityVal = 2;
            else if (s == "warn" || s == "warning") severityVal = 3;
            else if (s == "error") severityVal = 4;
            else if (s == "fatal" || s == "critical") severityVal = 5;
            else
            {
                const char* begin = s.c_str();
                char* endPtr = nullptr;
                long v = std::strtol(begin, &endPtr, 10);
                if (endPtr != begin && *endPtr == '\0')
                {
                    severityVal = static_cast<int>(v);
                }
            }
        }

        for (const auto& rule : m_rules)
        {
            if (!rule.enabled)
            {
                continue;
            }

            // Optional severity floor
            if (rule.minSeverity >= 0 && severityVal < rule.minSeverity)
            {
                continue;
            }

            // Choose target field; fallback to message
            std::string targetText;
            if (!rule.fieldKey.empty())
            {
                targetText = e.findByKey(rule.fieldKey);
            }
            if (targetText.empty())
            {
                targetText = message;
            }

            if (!boost::regex_search(targetText, rule.pattern))
            {
                matchedAll = false;
                break;
            }

            score += rule.weight;
            if (!rule.name.empty())
            {
                matchedReasons.push_back(rule.name);
            }
            matchedAll = true;

            // per-rule contribution
            const auto idx = &rule - &m_rules[0];
            perRule[static_cast<size_t>(idx)] = rule.weight;
        }

        // If rules exist and not all matched, skip this event
        if (!matchedAll && !m_rules.empty())
        {
            continue;
        }

        // Dummy Mahalanobis-like term using severity + message length
        std::vector<double> vals = {static_cast<double>(severityVal), static_cast<double>(message.size())};
        const auto distances = computeMahalanobis(vals);
        if (!distances.empty())
        {
            score += distances.front();
        }

        if (score > 0.0)
        {
            std::string reason;
            if (!matchedReasons.empty())
            {
                reason = matchedReasons.front();
                for (size_t idx = 1; idx < matchedReasons.size(); ++idx)
                {
                    reason += ", " + matchedReasons[idx];
                }
            }

            Result r;
            r.index = i;
            r.score = score;
            r.message = std::move(message);
            r.reason = std::move(reason);
            r.timestamp = std::move(timestamp);
            r.ruleScores = std::move(perRule);
            results.push_back(std::move(r));
        }
    }

    return results;
}

std::vector<std::string> MetricsEngine::GetRuleNames() const
{
    std::vector<std::string> names;
    names.reserve(m_rules.size());
    for (const auto& r : m_rules)
    {
        names.push_back(r.name);
    }
    return names;
}

std::vector<double> MetricsEngine::computeMahalanobis(const std::vector<double>& values) const
{
    if (values.empty())
    {
        return {};
    }

    Eigen::VectorXd x(values.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
        x(static_cast<Eigen::Index>(i)) = values[i];
    }

    // For the skeleton, use identity covariance; score equals norm.
    const double norm = x.norm();
    return {norm};
}
