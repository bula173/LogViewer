#pragma once

#include <QDateTime>
#include <QSet>
#include <QString>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace db
{
class EventsContainer;
class LogEvent;
} // namespace db

namespace ui::qt
{
class EventsTableModel;
}

/// Quick filters offered by the events table context menu: "show only" /
/// "exclude" a cell value and the "conversation" of two actors are built on
/// the column filters (composable, shown by the header funnel, cleared by
/// "Clear All Column Filters"); the time window narrows the upstream filter
/// (see EventsTableView::ShowTimeWindow).
namespace ui::qt::quick_filters
{

/// One column's value filter, as EventsTableModel stores it.
struct ColumnFilterSpec
{
    QSet<QString> values;          ///< shown values, or the hidden ones when @c exclude
    bool          exclude {false};

    bool operator==(const ColumnFilterSpec&) const = default;
};

/// The filter of a column that shows only cells listed in @p values *and*
/// still passing the @p current filter (nullopt: none) — it never widens it.
[[nodiscard]] ColumnFilterSpec ShowOnly(const std::optional<ColumnFilterSpec>& current,
                                        const QSet<QString>& values);

/// The @p current filter of a column that additionally hides @p value.
[[nodiscard]] ColumnFilterSpec Exclude(const std::optional<ColumnFilterSpec>& current,
                                       const QString& value);

/// ShowOnly / Exclude applied to @p column of @p model, combined with its filter.
void ApplyShowOnly(EventsTableModel& model, int column, const QSet<QString>& values);
void ApplyExclude(EventsTableModel& model, int column, const QString& value);

/// @p text for a menu entry: elided to @p maxChars, '&' kept literal.
[[nodiscard]] QString MenuText(const QString& text, int maxChars = 40);

// ── Time window ─────────────────────────────────────────────────────────────

/// Where an event sits in time: the field holding its timestamp and its value.
struct TimeAnchor
{
    std::string field;
    QDateTime   time;
};

/// The first of the usual timestamp fields of @p event that parses
/// (panel_utils::ParseTimestamp); nullopt if it has none.
[[nodiscard]] std::optional<TimeAnchor> FindTimeAnchor(const db::LogEvent& event);

/// Indices among @p candidates (every event when nullptr), in their order,
/// whose @p anchor field is within ±@p seconds of the anchor time, inclusive.
/// Events without a parseable timestamp are left out.
[[nodiscard]] std::vector<unsigned long> EventsInTimeWindow(
    db::EventsContainer& events, const std::vector<unsigned long>* candidates,
    const TimeAnchor& anchor, int seconds);

// ── Conversation ────────────────────────────────────────────────────────────

/// Model columns holding the sender and the receiver of a message.
struct ActorColumns
{
    int sender {-1};
    int receiver {-1};
};

/// Sender- and receiver-like columns among the column @p names (model order;
/// an empty name is never chosen). A name equal to a usual keyword ("source",
/// "destination", …) wins over one that only contains a keyword
/// (analyzer::ActorDiscoverer scores). nullopt unless both are found.
[[nodiscard]] std::optional<ActorColumns> FindActorColumns(const std::vector<QString>& names);

/// The actor pairs (sender, receiver) a row names: comma lists are fanned
/// out, placeholders ("internal", …) and self pairs dropped, and each pair is
/// listed once whatever its direction.
[[nodiscard]] std::vector<std::pair<QString, QString>> ConversationPairs(
    const QString& senderCell, const QString& receiverCell);

/// The cells of @p cells naming actor @p a or @p b (a comma list naming
/// either counts).
[[nodiscard]] QSet<QString> CellsNaming(const QSet<QString>& cells, const QString& a,
                                        const QString& b);

/// Restricts both actor columns to cells naming @p a or @p b, so the table
/// shows the messages between them in either direction (messages of one of
/// them to itself also pass).
void ApplyConversation(EventsTableModel& model, const ActorColumns& columns,
                       const QString& a, const QString& b);

} // namespace ui::qt::quick_filters
