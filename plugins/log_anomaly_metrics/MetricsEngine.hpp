#pragma once

#include "EventsContainer.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/regex.hpp>
#include <Eigen/Dense>
#include <memory>
#include <string>
#include <vector>

class MetricsEngine
{
public:
    struct Rule
    {
        std::string name;
        std::string fieldKey = "message"; // which field to inspect
        boost::regex pattern;
        double weight = 1.0;
        int minSeverity = -1; // optional lower bound; -1 = ignore
        bool enabled = true;
    };

    struct Result
    {
        size_t index = 0;
        double score = 0.0;
        std::string message;
        std::string reason;
        std::string timestamp;
        std::vector<double> ruleScores; // per-rule contribution
    };

    MetricsEngine();
    void SetEventsContainer(std::shared_ptr<db::EventsContainer> events);

    bool LoadConfig(const std::string& path);
    bool SaveConfig(const std::string& path) const;

    std::vector<Result> Analyze() const;
    std::vector<std::string> GetRuleNames() const;

    const std::vector<Rule>& GetRules() const { return m_rules; }
    void SetRules(std::vector<Rule> rules) { m_rules = std::move(rules); }

private:
    std::vector<double> computeMahalanobis(const std::vector<double>& values) const;

    std::shared_ptr<db::EventsContainer> m_events;
    std::vector<Rule> m_rules;
};
