#pragma once

#include "EventsContainer.hpp"
#include "events/EventsTableView.hpp"

#include <QDateTime>
#include <QString>

#include <string>
#include <vector>

namespace ui::qt::panel_utils {

inline const std::vector<std::string> kTsFields{
    "timestamp", "time", "datetime", "@timestamp", "date"};

inline const std::vector<std::string> kMsgFields{
    "message", "msg", "text", "description", "body"};

inline QDateTime ParseTimestamp(const QString& s)
{
    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (dt.isValid()) return dt;
    dt = QDateTime::fromString(s, Qt::ISODate);
    if (dt.isValid()) return dt;
    for (const char* fmt : {"yyyy-MM-dd HH:mm:ss.zzz",
                            "yyyy-MM-dd HH:mm:ss",
                            "dd/MMM/yyyy:HH:mm:ss"})
    {
        dt = QDateTime::fromString(s, QString::fromLatin1(fmt));
        if (dt.isValid()) return dt;
    }
    bool ok = false;
    const qint64 epoch = s.toLongLong(&ok);
    if (ok) return QDateTime::fromSecsSinceEpoch(epoch);
    return {};
}

inline std::vector<unsigned long> VisibleIndices(
    const EventsTableView* eventsView, const db::EventsContainer& events)
{
    // A non-null result means a filter is active — even if it matches zero
    // events, that must be honored, not treated as "no filter, show all".
    const std::vector<unsigned long>* f = eventsView->GetFilteredIndices();
    if (f) return *f;
    const size_t total = events.Size();
    std::vector<unsigned long> all;
    all.reserve(total);
    for (size_t i = 0; i < total; ++i)
        all.push_back(static_cast<unsigned long>(i));
    return all;
}

} // namespace ui::qt::panel_utils
