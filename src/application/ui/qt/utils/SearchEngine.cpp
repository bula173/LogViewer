#include "SearchEngine.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <QString>

namespace ui::qt::utils
{

SearchEngine::SearchEngine() = default;

void SearchEngine::SetMode(SearchMode mode)
{
    m_mode = mode;
}

bool SearchEngine::CompilePattern(const std::string& pattern, std::string& outError)
{
    m_pattern = pattern;

    switch (m_mode) {
        case SearchMode::Substring:
            return compileSimple(pattern);

        case SearchMode::CaseSensitive:
            m_caseSensitive = true;
            return compileSimple(pattern);

        case SearchMode::Regex:
            return compileRegex(pattern, outError);

        case SearchMode::Advanced:
            return compileAdvanced(pattern, outError);
    }

    return false;
}

bool SearchEngine::compileSimple(const std::string&)
{
    m_compiledRegex.reset();
    m_advancedQuery.reset();
    return true;  // Simple patterns always compile
}

bool SearchEngine::compileRegex(const std::string& pattern, std::string& outError)
{
    try {
        std::regex::flag_type flags = std::regex::ECMAScript;
        if (!m_caseSensitive)
            flags |= std::regex::icase;

        m_compiledRegex = std::regex(pattern, flags);
        m_advancedQuery.reset();
        return true;
    }
    catch (const std::regex_error& e) {
        outError = std::string("Regex error: ") + e.what();
        return false;
    }
}

bool SearchEngine::compileAdvanced(const std::string& pattern, std::string& outError)
{
    m_compiledRegex.reset();
    AdvancedQuery query;

    std::istringstream stream(pattern);
    std::string token;
    std::string currentOp = "AND";  // default operator

    while (stream >> token) {
        if (token == "AND" || token == "and") {
            currentOp = "AND";
        } else if (token == "OR" || token == "or") {
            currentOp = "OR";
        } else if (token == "NOT" || token == "not") {
            currentOp = "NOT";
        } else {
            // Regular term
            if (currentOp == "NOT") {
                query.notTerms.push_back(token);
            } else if (currentOp == "OR") {
                query.orTerms.push_back(token);
            } else {
                query.andTerms.push_back(token);
            }
            currentOp = "AND";  // reset to default
        }
    }

    if (query.andTerms.empty() && query.orTerms.empty() && query.notTerms.empty()) {
        outError = "Advanced query is empty";
        return false;
    }

    m_advancedQuery = query;
    return true;
}

bool SearchEngine::Matches(const std::string& text) const
{
    if (m_pattern.empty())
        return true;

    switch (m_mode) {
        case SearchMode::Substring:
        case SearchMode::CaseSensitive:
            return matchesSimple(text);

        case SearchMode::Regex:
            return matchesRegex(text);

        case SearchMode::Advanced:
            return matchesAdvanced(text);
    }

    return false;
}

bool SearchEngine::matchesSimple(const std::string& text) const
{
    if (m_caseSensitive) {
        return text.find(m_pattern) != std::string::npos;
    }

    // Case-insensitive search - avoid repeated allocations by caching normalized pattern
    if (!m_normalizedPatternValid) {
        m_normalizedPattern = m_pattern;
        std::transform(m_normalizedPattern.begin(), m_normalizedPattern.end(),
                      m_normalizedPattern.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        m_normalizedPatternValid = true;
    }

    // Fast path: if pattern is ASCII, we can use inline lowercasing
    std::string lower_text;
    lower_text.reserve(text.length());
    for (unsigned char c : text) {
        lower_text += static_cast<char>(std::tolower(static_cast<int>(c)));
    }

    return lower_text.find(m_normalizedPattern) != std::string::npos;
}

bool SearchEngine::matchesRegex(const std::string& text) const
{
    if (!m_compiledRegex)
        return false;

    try {
        return std::regex_search(text, *m_compiledRegex);
    }
    catch (...) {
        return false;
    }
}

bool SearchEngine::matchesAdvanced(const std::string& text) const
{
    if (!m_advancedQuery)
        return false;

    const auto& query = *m_advancedQuery;
    const Qt::CaseSensitivity cs = m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

    // All AND terms must be present
    for (const auto& term : query.andTerms) {
        QString text_q = QString::fromStdString(text);
        QString term_q = QString::fromStdString(term);
        if (!text_q.contains(term_q, cs)) {
            return false;
        }
    }

    // At least one OR term must be present (if OR terms exist)
    if (!query.orTerms.empty()) {
        bool foundOr = false;
        for (const auto& term : query.orTerms) {
            QString text_q = QString::fromStdString(text);
            QString term_q = QString::fromStdString(term);
            if (text_q.contains(term_q, cs)) {
                foundOr = true;
                break;
            }
        }
        if (!foundOr)
            return false;
    }

    // No NOT terms must be present
    for (const auto& term : query.notTerms) {
        QString text_q = QString::fromStdString(text);
        QString term_q = QString::fromStdString(term);
        if (text_q.contains(term_q, cs)) {
            return false;
        }
    }

    return true;
}

std::vector<SearchMatch> SearchEngine::FindMatches(const std::string& text) const
{
    if (m_pattern.empty())
        return {};

    switch (m_mode) {
        case SearchMode::Substring:
        case SearchMode::CaseSensitive:
            return findSimpleMatches(text);

        case SearchMode::Regex:
            return findRegexMatches(text);

        case SearchMode::Advanced:
            // For advanced queries, just highlight the first term
            if (m_advancedQuery && !m_advancedQuery->andTerms.empty()) {
                SearchEngine simple;
                simple.SetMode(SearchMode::Substring);
                simple.SetCaseSensitive(m_caseSensitive);
                std::string error;
                simple.CompilePattern(m_advancedQuery->andTerms[0], error);
                return simple.FindMatches(text);
            }
            return {};
    }

    return {};
}

std::vector<SearchMatch> SearchEngine::findSimpleMatches(const std::string& text) const
{
    std::vector<SearchMatch> matches;
    matches.reserve(4);  // Most texts have few matches

    if (m_caseSensitive) {
        size_t pos = 0;
        while ((pos = text.find(m_pattern, pos)) != std::string::npos) {
            matches.push_back({static_cast<int>(pos), static_cast<int>(pos + m_pattern.length())});
            pos += m_pattern.length();
        }
    } else {
        // Ensure normalized pattern is ready
        if (!m_normalizedPatternValid) {
            m_normalizedPattern = m_pattern;
            std::transform(m_normalizedPattern.begin(), m_normalizedPattern.end(),
                          m_normalizedPattern.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            m_normalizedPatternValid = true;
        }

        std::string lower_text;
        lower_text.reserve(text.length());
        for (unsigned char c : text) {
            lower_text += static_cast<char>(std::tolower(static_cast<int>(c)));
        }

        size_t pos = 0;
        while ((pos = lower_text.find(m_normalizedPattern, pos)) != std::string::npos) {
            matches.push_back({static_cast<int>(pos), static_cast<int>(pos + m_normalizedPattern.length())});
            pos += m_normalizedPattern.length();
        }
    }

    return matches;
}

std::vector<SearchMatch> SearchEngine::findRegexMatches(const std::string& text) const
{
    std::vector<SearchMatch> matches;

    if (!m_compiledRegex)
        return matches;

    try {
        std::sregex_iterator iter(text.begin(), text.end(), *m_compiledRegex);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            matches.push_back({
                static_cast<int>(iter->position()),
                static_cast<int>(iter->position() + iter->length())
            });
        }
    }
    catch (...) {
        // Ignore regex errors during match finding
    }

    return matches;
}

void SearchEngine::Clear()
{
    m_pattern.clear();
    m_compiledRegex.reset();
    m_advancedQuery.reset();
}

void SearchEngine::AddToHistory(const std::string& term)
{
    if (term.empty())
        return;

    // Remove if already exists (avoid duplicates)
    auto it = std::find(m_history.begin(), m_history.end(), term);
    if (it != m_history.end()) {
        m_history.erase(it);
    }

    // Add to front (most recent first)
    m_history.insert(m_history.begin(), term);

    // Trim history if too large
    if (m_history.size() > kMaxHistorySize) {
        m_history.resize(kMaxHistorySize);
    }
}

std::vector<std::string> SearchEngine::GetSuggestions(const std::string& prefix) const
{
    std::vector<std::string> suggestions;

    for (const auto& item : m_history) {
        if (item.substr(0, std::min(prefix.length(), item.length())) == prefix) {
            suggestions.push_back(item);
            if (suggestions.size() >= 10)  // Limit to 10 suggestions
                break;
        }
    }

    return suggestions;
}

}  // namespace ui::qt::utils
