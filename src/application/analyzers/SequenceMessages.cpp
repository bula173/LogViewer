#include "SequenceMessages.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace analyzer
{

namespace
{

constexpr std::array<std::string_view, 4> kPlaceholders = {"internal", "none", "n/a", "-"};

std::string_view Trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b)
{
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) ==
                      std::tolower(static_cast<unsigned char>(y));
           });
}

/// Real actors named by a sender/receiver value (placeholders dropped).
std::vector<std::string> ActorsIn(std::string_view value)
{
    auto parts = SplitActorList(value);
    std::erase_if(parts, [](const std::string& p) { return IsPlaceholderActor(p); });
    return parts;
}

} // namespace

bool IsPlaceholderActor(std::string_view name)
{
    name = Trim(name);
    return std::any_of(kPlaceholders.begin(), kPlaceholders.end(),
                       [&](std::string_view p) { return EqualsIgnoreCase(name, p); });
}

std::vector<std::string> SplitActorList(std::string_view value)
{
    std::vector<std::string> parts;
    while (!value.empty())
    {
        const auto comma = value.find(',');
        const auto part  = Trim(value.substr(0, comma));
        if (!part.empty())
            parts.emplace_back(part);
        if (comma == std::string_view::npos)
            break;
        value.remove_prefix(comma + 1);
    }
    return parts;
}

std::vector<SequenceMessage> CollectSequenceMessages(
    db::EventsContainer& events, const ExchangePattern& pattern, std::size_t limit)
{
    std::vector<SequenceMessage> out;
    if (pattern.senderField.empty() || pattern.receiverField.empty() || limit == 0)
        return out;

    for (std::size_t i = 0; i < events.Size() && out.size() < limit; ++i)
    {
        const auto& ev = events.GetEvent(i);
        const auto senders   = ActorsIn(ev.findByKey(pattern.senderField));
        const auto receivers = ActorsIn(ev.findByKey(pattern.receiverField));
        if (senders.empty() || receivers.empty())
            continue;

        const std::string label = pattern.labelField.empty()
                                      ? std::string{}
                                      : ev.findByKey(pattern.labelField);

        for (const auto& from : senders)
            for (const auto& to : receivers)
            {
                if (out.size() >= limit)
                    return out;
                out.push_back({from, to, label, i});
            }
    }
    return out;
}

} // namespace analyzer
