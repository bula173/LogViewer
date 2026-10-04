#include "QuickFilters.hpp"

#include "EventsContainer.hpp"
#include "EventsTableModel.hpp"
#include "LogEvent.hpp"
#include "analyzers/ActorDiscoverer.hpp"
#include "analyzers/SequenceMessages.hpp"
#include "utils/PanelUtils.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <set>

namespace ui::qt::quick_filters
{

ColumnFilterSpec ShowOnly(const std::optional<ColumnFilterSpec>& current,
                          const QSet<QString>& values)
{
    if (!current)
        return {values, false};
    QSet<QString> shown = values;
    if (current->exclude)
        shown.subtract(current->values);
    else
        shown.intersect(current->values);
    return {shown, false};
}

ColumnFilterSpec Exclude(const std::optional<ColumnFilterSpec>& current, const QString& value)
{
    if (!current)
        return {{value}, true};
    ColumnFilterSpec next = *current;
    if (next.exclude)
        next.values.insert(value);
    else
        next.values.remove(value);
    return next;
}

namespace
{
std::optional<ColumnFilterSpec> CurrentFilter(const EventsTableModel& model, int column)
{
    if (!model.HasColumnFilter(column))
        return std::nullopt;
    return ColumnFilterSpec{model.ColumnFilterValues(column), model.IsColumnFilterExclusion(column)};
}

void Store(EventsTableModel& model, int column, const ColumnFilterSpec& filter)
{
    if (filter.exclude)
        model.SetColumnFilterExcluding(column, filter.values);
    else
        model.SetColumnFilter(column, filter.values);
}
} // namespace

void ApplyShowOnly(EventsTableModel& model, int column, const QSet<QString>& values)
{
    Store(model, column, ShowOnly(CurrentFilter(model, column), values));
}

void ApplyExclude(EventsTableModel& model, int column, const QString& value)
{
    Store(model, column, Exclude(CurrentFilter(model, column), value));
}

QString MenuText(const QString& text, int maxChars)
{
    QString out = text.simplified(); // no line breaks in a menu entry
    if (out.size() > maxChars)
        out = out.left(maxChars - 1) + QChar(0x2026); // …
    return out.replace('&', QStringLiteral("&&"));
}

// ── Time window ─────────────────────────────────────────────────────────────

std::optional<TimeAnchor> FindTimeAnchor(const db::LogEvent& event)
{
    for (const auto& field : panel_utils::kTsFields)
    {
        const std::string value = event.findByKey(field);
        if (value.empty())
            continue;
        const QDateTime time = panel_utils::ParseTimestamp(QString::fromStdString(value));
        if (time.isValid())
            return TimeAnchor{field, time};
    }
    return std::nullopt;
}

std::vector<unsigned long> EventsInTimeWindow(db::EventsContainer& events,
                                              const std::vector<unsigned long>* candidates,
                                              const TimeAnchor& anchor, int seconds)
{
    const qint64 windowMs = static_cast<qint64>(seconds) * 1000;
    const std::size_t total = events.Size();
    std::vector<unsigned long> out;
    auto consider = [&](unsigned long idx) {
        if (idx >= total)
            return;
        const QDateTime time = panel_utils::ParseTimestamp(
            QString::fromStdString(events.GetEvent(idx).findByKey(anchor.field)));
        if (time.isValid() && std::abs(anchor.time.msecsTo(time)) <= windowMs)
            out.push_back(idx);
    };
    if (candidates)
        std::for_each(candidates->begin(), candidates->end(), consider);
    else
        for (unsigned long idx = 0; idx < total; ++idx)
            consider(idx);
    return out;
}

// ── Conversation ────────────────────────────────────────────────────────────

std::optional<ActorColumns> FindActorColumns(const std::vector<QString>& names)
{
    static const std::array<const char*, 4> kSenderNames   = {"source", "src", "sender", "from"};
    static const std::array<const char*, 5> kReceiverNames = {"destination", "dest", "receiver",
                                                              "target", "to"};
    auto find = [&names](const auto& exactNames, auto score, auto otherScore) {
        for (int column = 0; column < static_cast<int>(names.size()); ++column)
            for (const char* exact : exactNames)
                if (names[static_cast<std::size_t>(column)].compare(
                        QLatin1String(exact), Qt::CaseInsensitive) == 0)
                    return column;
        for (int column = 0; column < static_cast<int>(names.size()); ++column)
        {
            const std::string name = names[static_cast<std::size_t>(column)].toStdString();
            if (!name.empty() && score(name) > 0 && otherScore(name) == 0)
                return column;
        }
        return -1;
    };
    const int sender = find(kSenderNames, analyzer::ActorDiscoverer::ScoreSender,
                            analyzer::ActorDiscoverer::ScoreReceiver);
    const int receiver = find(kReceiverNames, analyzer::ActorDiscoverer::ScoreReceiver,
                              analyzer::ActorDiscoverer::ScoreSender);
    if (sender < 0 || receiver < 0 || sender == receiver)
        return std::nullopt;
    return ActorColumns{sender, receiver};
}

namespace
{
/// Real actors named by a sender/receiver cell (placeholders dropped).
std::vector<std::string> ActorsIn(const QString& cell)
{
    auto actors = analyzer::SplitActorList(cell.toStdString());
    std::erase_if(actors, [](const std::string& a) { return analyzer::IsPlaceholderActor(a); });
    return actors;
}
} // namespace

std::vector<std::pair<QString, QString>> ConversationPairs(const QString& senderCell,
                                                           const QString& receiverCell)
{
    std::vector<std::pair<QString, QString>> pairs;
    std::set<std::pair<std::string, std::string>> seen; // unordered: smaller name first
    for (const auto& from : ActorsIn(senderCell))
        for (const auto& to : ActorsIn(receiverCell))
        {
            if (from == to || !seen.insert(std::minmax(from, to)).second)
                continue;
            pairs.emplace_back(QString::fromStdString(from), QString::fromStdString(to));
        }
    return pairs;
}

QSet<QString> CellsNaming(const QSet<QString>& cells, const QString& a, const QString& b)
{
    const std::string first  = a.toStdString();
    const std::string second = b.toStdString();
    QSet<QString> out;
    for (const QString& cell : cells)
    {
        const auto actors = ActorsIn(cell);
        if (std::any_of(actors.begin(), actors.end(),
                        [&](const std::string& n) { return n == first || n == second; }))
            out.insert(cell);
    }
    return out;
}

void ApplyConversation(EventsTableModel& model, const ActorColumns& columns, const QString& a,
                       const QString& b)
{
    ApplyShowOnly(model, columns.sender, CellsNaming(model.AllColumnValues(columns.sender), a, b));
    ApplyShowOnly(model, columns.receiver,
                  CellsNaming(model.AllColumnValues(columns.receiver), a, b));
}

} // namespace ui::qt::quick_filters
