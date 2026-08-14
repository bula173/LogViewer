#include "CanStatisticsStrategy.hpp"

#include "EventsContainer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace ui::qt {

namespace {

double ParseDouble(const std::string& s)
{
    if (s.empty()) return std::numeric_limits<double>::quiet_NaN();
    try { return std::stod(s); } catch (...) { return std::numeric_limits<double>::quiet_NaN(); }
}

struct Accum {
    double mn  {(std::numeric_limits<double>::max)()};
    double mx  {std::numeric_limits<double>::lowest()};
    double sum {0.0};
    size_t n   {0};

    void add(double v) {
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
        ++n;
    }
    double avg() const { return n > 0 ? sum / static_cast<double>(n) : 0.0; }
};

std::string FmtPct(size_t count, size_t total)
{
    if (total == 0) return "0%";
    const double p = 100.0 * static_cast<double>(count) / static_cast<double>(total);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f%%", p);
    return buf;
}

std::string FmtF(double v, int decimals = 3)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

} // namespace

bool CanStatisticsStrategy::Matches(db::EventsContainer& events) const
{
    const size_t probe = (std::min)(events.Size(), size_t{20});
    for (size_t i = 0; i < probe; ++i)
    {
        if (!events.GetEvent(i).findByKey("CAN_ID").empty())
            return true;
    }
    return false;
}

std::vector<StatsSection> CanStatisticsStrategy::Compute(
    db::EventsContainer& events,
    const std::vector<unsigned long>& indices) const
{
    if (indices.empty()) return {};

    size_t rxCount   = 0;
    size_t txCount   = 0;
    size_t txRqCount = 0;
    size_t errCount  = 0;
    std::set<std::string> uniqueIds;
    std::set<std::string> channels;
    double tsMin = (std::numeric_limits<double>::max)();
    double tsMax = std::numeric_limits<double>::lowest();
    std::map<std::string, Accum> sigAccum;

    for (unsigned long idx : indices)
    {
        const auto& ev = events.GetEvent(idx);

        const std::string type = ev.findByKey("type");
        if      (type == "Rx")         ++rxCount;
        else if (type == "Tx")         ++txCount;
        else if (type == "TxRq")       ++txRqCount;
        else if (type == "ErrorFrame") ++errCount;

        const std::string id = ev.findByKey("CAN_ID");
        if (!id.empty()) uniqueIds.insert(id);

        const std::string ch = ev.findByKey("CAN_Channel");
        if (!ch.empty()) channels.insert(ch);

        const double ts = ParseDouble(ev.findByKey("timestamp"));
        if (!std::isnan(ts)) {
            tsMin = (std::min)(tsMin, ts);
            tsMax = (std::max)(tsMax, ts);
        }

        for (const auto& [key, val] : ev.getEventItems())
        {
            if (key.size() > 4 && key.substr(0, 4) == "SIG:")
            {
                const double v = ParseDouble(val);
                if (!std::isnan(v))
                    sigAccum[key].add(v);
            }
        }
    }

    const size_t total = indices.size();
    auto withPct = [&](size_t n) -> std::string {
        return std::to_string(n) + "  (" + FmtPct(n, total) + ")";
    };

    // ── Section 1: CAN Bus Summary ────────────────────────────────────────
    StatsSection summary;
    summary.title = "CAN Bus Summary";
    summary.rows.push_back({"Total frames",    std::to_string(total)});
    if (rxCount  > 0) summary.rows.push_back({"Rx frames",   withPct(rxCount)});
    if (txCount  > 0) summary.rows.push_back({"Tx frames",   withPct(txCount)});
    if (txRqCount> 0) summary.rows.push_back({"TxRq frames", withPct(txRqCount)});
    summary.rows.push_back({"Error frames",    withPct(errCount)});
    summary.rows.push_back({"Unique CAN IDs",  std::to_string(uniqueIds.size())});

    if (!channels.empty()) {
        std::string chStr;
        for (const auto& c : channels) { if (!chStr.empty()) chStr += ", "; chStr += c; }
        summary.rows.push_back({"Channels", chStr});
    }

    const bool hasTime = (tsMin <= tsMax);
    if (hasTime) {
        const double duration = tsMax - tsMin;
        summary.rows.push_back({"Duration", FmtF(duration) + " s"});
        if (duration > 0.0)
            summary.rows.push_back({"Frame rate",
                FmtF(static_cast<double>(total) / duration, 1) + " frames/s"});
    }

    std::vector<StatsSection> result;
    result.push_back(std::move(summary));

    // ── Section 2: Signal Ranges ──────────────────────────────────────────
    if (!sigAccum.empty()) {
        StatsSection sigSec;
        sigSec.title = "Signal Ranges";
        size_t shown = 0;
        for (const auto& [key, acc] : sigAccum) {
            if (shown >= 15) {
                sigSec.rows.push_back({"(and more...)", ""});
                break;
            }
            const std::string name = key.substr(4);
            const std::string val  = "min " + FmtF(acc.mn)
                                   + "   max " + FmtF(acc.mx)
                                   + "   avg " + FmtF(acc.avg());
            sigSec.rows.push_back({name, val});
            ++shown;
        }
        result.push_back(std::move(sigSec));
    }

    return result;
}

} // namespace ui::qt
