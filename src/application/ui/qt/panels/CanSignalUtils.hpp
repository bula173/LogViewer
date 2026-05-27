#pragma once

#include "EventsContainer.hpp"

#include <QString>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace ui::qt {

/// One CAN frame and all signal keys observed inside it.
struct FrameSignals {
    std::string              canId;      ///< raw CAN_ID field value
    std::string              msgName;    ///< CAN_MsgName (from DBC); empty if unknown
    std::vector<std::string> signalKeys; ///< full "SIG:xxx" keys, sorted
};

/// Scan all events and return one FrameSignals per unique CAN_ID, sorted by ID.
/// Only events that contain a CAN_ID field are considered.
inline std::vector<FrameSignals> BuildFrameSignalTree(db::EventsContainer& events)
{
    std::map<std::string, FrameSignals> frameMap;
    const size_t total = events.Size();

    for (size_t i = 0; i < total; ++i)
    {
        const auto& ev  = events.GetItem(i);
        const std::string canId = ev.findByKey("CAN_ID");
        if (canId.empty()) continue;

        auto& fs = frameMap[canId];
        if (fs.canId.empty())
        {
            fs.canId   = canId;
            fs.msgName = ev.findByKey("CAN_MsgName");
        }

        for (const auto& [key, val] : ev.getEventItems())
        {
            if (key.size() > 4 && key.substr(0, 4) == "SIG:")
            {
                auto pos = std::lower_bound(fs.signalKeys.begin(), fs.signalKeys.end(), key);
                if (pos == fs.signalKeys.end() || *pos != key)
                    fs.signalKeys.insert(pos, key);
            }
        }
    }

    std::vector<FrameSignals> result;
    result.reserve(frameMap.size());
    for (auto& [id, fs] : frameMap)
        result.push_back(std::move(fs));
    return result;
}

/// Format a raw CAN_ID string as "0xNNN" (uppercase hex).
/// Falls back to the raw string if it cannot be parsed as an integer.
inline QString FormatCanId(const std::string& canId)
{
    bool ok = false;
    const uint32_t num = QString::fromStdString(canId).toUInt(&ok);
    return ok ? QString("0x%1").arg(num, 3, 16, QChar('0')).toUpper()
              : QString::fromStdString(canId);
}

/// Build a human-readable frame label: "MsgName  (0xNNN)" or just "0xNNN".
inline QString FormatFrameLabel(const FrameSignals& fs)
{
    const QString hexId = FormatCanId(fs.canId);
    if (!fs.msgName.empty())
        return QString::fromStdString(fs.msgName) + "  (" + hexId + ')';
    return hexId;
}

} // namespace ui::qt
