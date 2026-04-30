#include "PatternAnalysisPanel.hpp"

#include "Config.hpp"
#include "EventsContainer.hpp"
#include "EventsTableView.hpp"

#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ui::qt {

// ============================================================================
// Construction
// ============================================================================

PatternAnalysisPanel::PatternAnalysisPanel(db::EventsContainer& events,
                                           EventsTableView*     eventsView,
                                           QWidget*             parent)
    : QWidget(parent), m_events(events), m_eventsView(eventsView)
{
    BuildLayout();
}

void PatternAnalysisPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // ── Refresh button ────────────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout();
        auto* btn = new QPushButton(tr("Analyse"), this);
        btn->setToolTip(tr("Re-run all pattern analyses on the currently visible events"));
        connect(btn, &QPushButton::clicked, this, &PatternAnalysisPanel::Refresh);
        row->addStretch();
        row->addWidget(btn);
        layout->addLayout(row);
    }

    // ── Main tabs ─────────────────────────────────────────────────────────
    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs);

    // ── Tab 1: Message Templates ──────────────────────────────────────────
    {
        auto* w  = new QWidget();
        auto* vl = new QVBoxLayout(w);

        auto* ctrlRow = new QHBoxLayout();
        ctrlRow->addWidget(new QLabel(tr("Message field:"), w));
        m_msgFieldCombo = new QComboBox(w);
        m_msgFieldCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_msgFieldCombo->setToolTip(
            tr("Choose the log field whose text will be clustered into templates"));
        ctrlRow->addWidget(m_msgFieldCombo);
        vl->addLayout(ctrlRow);

        m_templateTable = new QTableWidget(0, 4, w);
        m_templateTable->setHorizontalHeaderLabels(
            {tr("Type"), tr("Template"), tr("Count"), tr("%")});
        m_templateTable->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::ResizeToContents);
        m_templateTable->horizontalHeader()->setSectionResizeMode(
            1, QHeaderView::Stretch);
        m_templateTable->horizontalHeader()->setSectionResizeMode(
            2, QHeaderView::ResizeToContents);
        m_templateTable->horizontalHeader()->setSectionResizeMode(
            3, QHeaderView::ResizeToContents);
        m_templateTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_templateTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_templateTable->verticalHeader()->hide();
        m_templateTable->setAlternatingRowColors(true);
        m_templateTable->setWordWrap(false);
        m_templateTable->setToolTip(
            tr("Click a row to highlight matching events in the event table"));
        vl->addWidget(m_templateTable);

        connect(m_templateTable, &QTableWidget::cellClicked,
                this, [this](int row, int) {
                    if (row >= 0 &&
                        row < static_cast<int>(m_templateMatches.size()))
                    {
                        emit TemplateSelected(m_templateMatches[static_cast<size_t>(row)]);
                    }
                });

        m_tabs->addTab(w, tr("Message Templates"));
    }

    // ── Tab 2: Co-occurrence ──────────────────────────────────────────────
    {
        auto* w  = new QWidget();
        auto* vl = new QVBoxLayout(w);

        auto* infoLabel = new QLabel(
            tr("Shows pairs of event types that frequently appear within the same "
               "time window — useful for spotting causal or correlated sequences."), w);
        infoLabel->setWordWrap(true);
        vl->addWidget(infoLabel);

        auto* ctrlRow = new QHBoxLayout();
        ctrlRow->addWidget(new QLabel(tr("Time window (s):"), w));
        m_windowSpin = new QSpinBox(w);
        m_windowSpin->setRange(1, 3600);
        m_windowSpin->setValue(5);
        m_windowSpin->setToolTip(
            tr("Two events are considered co-occurring if their timestamps are "
               "within this many seconds of each other"));
        ctrlRow->addWidget(m_windowSpin);
        ctrlRow->addStretch();
        vl->addLayout(ctrlRow);

        m_coocTable = new QTableWidget(0, 3, w);
        m_coocTable->setHorizontalHeaderLabels(
            {tr("Type A"), tr("Type B"), tr("Co-occurrences")});
        m_coocTable->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::Stretch);
        m_coocTable->horizontalHeader()->setSectionResizeMode(
            1, QHeaderView::Stretch);
        m_coocTable->horizontalHeader()->setSectionResizeMode(
            2, QHeaderView::ResizeToContents);
        m_coocTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_coocTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_coocTable->verticalHeader()->hide();
        m_coocTable->setAlternatingRowColors(true);
        vl->addWidget(m_coocTable);

        m_tabs->addTab(w, tr("Co-occurrence"));
    }

    // ── Tab 3: Sequence N-grams ────────────────────────────────────────────
    {
        auto* w  = new QWidget();
        auto* vl = new QVBoxLayout(w);

        auto* infoLabel = new QLabel(
            tr("Finds the most frequent consecutive sequences of event types "
               "(bigrams or trigrams) in chronological order."), w);
        infoLabel->setWordWrap(true);
        vl->addWidget(infoLabel);

        auto* ctrlRow = new QHBoxLayout();
        ctrlRow->addWidget(new QLabel(tr("N:"), w));
        m_ngramSizeCombo = new QComboBox(w);
        m_ngramSizeCombo->addItem(tr("Bigrams (2)"),  2);
        m_ngramSizeCombo->addItem(tr("Trigrams (3)"), 3);
        m_ngramSizeCombo->addItem(tr("4-grams"),      4);
        ctrlRow->addWidget(m_ngramSizeCombo);

        ctrlRow->addWidget(new QLabel(tr("  Top:"), w));
        m_ngramTopNSpin = new QSpinBox(w);
        m_ngramTopNSpin->setRange(5, 200);
        m_ngramTopNSpin->setValue(30);
        ctrlRow->addWidget(m_ngramTopNSpin);
        ctrlRow->addStretch();
        vl->addLayout(ctrlRow);

        m_ngramTable = new QTableWidget(0, 3, w);
        m_ngramTable->setHorizontalHeaderLabels(
            {tr("Sequence"), tr("Count"), tr("%")});
        m_ngramTable->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::Stretch);
        m_ngramTable->horizontalHeader()->setSectionResizeMode(
            1, QHeaderView::ResizeToContents);
        m_ngramTable->horizontalHeader()->setSectionResizeMode(
            2, QHeaderView::ResizeToContents);
        m_ngramTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_ngramTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_ngramTable->verticalHeader()->hide();
        m_ngramTable->setAlternatingRowColors(true);
        vl->addWidget(m_ngramTable);

        m_tabs->addTab(w, tr("Sequence N-grams"));
    }

    setLayout(layout);
}

// ============================================================================
// Public slot
// ============================================================================

void PatternAnalysisPanel::Refresh()
{
    // Repopulate message-field combo from first event's keys
    {
        m_msgFieldCombo->blockSignals(true);
        const QString current = m_msgFieldCombo->currentText();
        m_msgFieldCombo->clear();
        if (m_events.Size() > 0)
        {
            for (const auto& [key, val] : m_events.GetEvent(0).getEventItems())
                m_msgFieldCombo->addItem(QString::fromStdString(key));
        }
        const int idx = m_msgFieldCombo->findText(current);
        // Default to a sensible message field
        if (idx >= 0)
            m_msgFieldCombo->setCurrentIndex(idx);
        else
        {
            for (const char* hint : {"info", "message", "msg", "description", "text"})
            {
                const int i = m_msgFieldCombo->findText(QString::fromLatin1(hint));
                if (i >= 0) { m_msgFieldCombo->setCurrentIndex(i); break; }
            }
        }
        m_msgFieldCombo->blockSignals(false);
    }

    const auto indices = VisibleIndices();
    RefreshTemplates(indices);
    RefreshCooccurrence(indices);
    RefreshNgrams(indices);
}

// ============================================================================
// Helpers
// ============================================================================

std::vector<unsigned long> PatternAnalysisPanel::VisibleIndices() const
{
    const std::vector<unsigned long>* filtered = m_eventsView->GetFilteredIndices();
    if (filtered && !filtered->empty())
        return *filtered;

    const size_t total = m_events.Size();
    std::vector<unsigned long> all;
    all.reserve(total);
    for (size_t i = 0; i < total; ++i)
        all.push_back(static_cast<unsigned long>(i));
    return all;
}

std::vector<std::string> PatternAnalysisPanel::Tokenise(const std::string& text)
{
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : text)
    {
        // Split on whitespace and a few punctuation chars that vary between
        // instances (e.g. IDs, numbers, addresses)
        if (c == ' ' || c == '\t' || c == '\n' || c == ',' || c == ';')
        {
            if (!cur.empty()) { tokens.push_back(std::move(cur)); cur.clear(); }
        }
        else
        {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(std::move(cur));
    return tokens;
}

std::vector<std::string> PatternAnalysisPanel::MergeTokens(
    const std::vector<std::string>& a,
    const std::vector<std::string>& b)
{
    // If lengths differ too much treat entire message as variable
    if (a.size() != b.size())
        return {"{*}"};

    std::vector<std::string> result;
    result.reserve(a.size());
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (a[i] == b[i] || a[i] == "{*}" || b[i] == "{*}")
            result.push_back(a[i] == "{*}" ? a[i] : (b[i] == "{*}" ? b[i] : a[i]));
        else
        {
            // Heuristic: if one of the two tokens looks like a pure number or
            // hex/UUID, mark the position variable regardless
            result.push_back("{*}");
        }
    }
    return result;
}

QString PatternAnalysisPanel::RenderTemplate(const std::vector<std::string>& tokens)
{
    QString out;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        if (i) out += ' ';
        out += QString::fromStdString(tokens[i]);
    }
    return out;
}

// ============================================================================
// Tab 1 — Message Templates
// ============================================================================

void PatternAnalysisPanel::RefreshTemplates(
    const std::vector<unsigned long>& indices)
{
    m_templateMatches.clear();
    m_templateTable->setRowCount(0);

    if (indices.empty() || m_events.Size() == 0) return;

    const std::string msgField  =
        m_msgFieldCombo->currentText().toStdString();
    const std::string& typeField = config::GetConfig().typeFilterField;
    const size_t total = indices.size();

    // Group event indices by their type value
    std::map<std::string, std::vector<unsigned long>> byType;
    for (unsigned long idx : indices)
    {
        const std::string t =
            m_events.GetEvent(static_cast<int>(idx)).findByKey(typeField);
        byType[t.empty() ? "(none)" : t].push_back(idx);
    }

    struct TemplateEntry
    {
        std::string              typeName;
        std::vector<std::string> tokens;   // template token sequence
        size_t                   count{};
        std::vector<unsigned long> matching;
    };

    std::vector<TemplateEntry> entries;

    for (auto& [typeName, typeIndices] : byType)
    {
        // Within the type group, build templates by iteratively merging
        // We cluster greedily: each new message is compared to all existing
        // templates; if it shares ≥ 50 % constant tokens with one, merge.
        // Otherwise start a new template.
        std::vector<TemplateEntry> clusters;

        for (unsigned long idx : typeIndices)
        {
            const std::string msg =
                m_events.GetEvent(static_cast<int>(idx)).findByKey(msgField);
            const std::vector<std::string> toks = Tokenise(msg);

            // Find best matching cluster
            int    bestCluster = -1;
            double bestScore   = 0.0;

            for (int ci = 0; ci < static_cast<int>(clusters.size()); ++ci)
            {
                const auto& ctoks = clusters[static_cast<size_t>(ci)].tokens;
                if (ctoks.size() != toks.size()) continue;

                // Score = fraction of constant (non-{*}) positions that match
                int constants = 0, matches = 0;
                for (size_t ti = 0; ti < toks.size(); ++ti)
                {
                    if (ctoks[ti] != "{*}")
                    {
                        ++constants;
                        if (ctoks[ti] == toks[ti]) ++matches;
                    }
                }
                const double score =
                    constants > 0 ? static_cast<double>(matches) / constants : 0.5;
                if (score >= 0.5 && score > bestScore)
                {
                    bestScore   = score;
                    bestCluster = ci;
                }
            }

            if (bestCluster >= 0)
            {
                auto& c = clusters[static_cast<size_t>(bestCluster)];
                c.tokens = MergeTokens(c.tokens, toks);
                ++c.count;
                c.matching.push_back(idx);
            }
            else
            {
                TemplateEntry e;
                e.typeName = typeName;
                e.tokens   = toks;
                e.count    = 1;
                e.matching.push_back(idx);
                clusters.push_back(std::move(e));
            }
        }

        for (auto& c : clusters)
            entries.push_back(std::move(c));
    }

    // Sort by count descending
    std::sort(entries.begin(), entries.end(),
              [](const TemplateEntry& a, const TemplateEntry& b) {
                  return a.count > b.count;
              });

    m_templateTable->setRowCount(static_cast<int>(entries.size()));

    for (int r = 0; r < static_cast<int>(entries.size()); ++r)
    {
        const auto& e = entries[static_cast<size_t>(r)];
        const double pct =
            total > 0 ? 100.0 * e.count / static_cast<double>(total) : 0.0;

        const QString templateStr = RenderTemplate(e.tokens);

        auto* typeItem = new QTableWidgetItem(
            QString::fromStdString(e.typeName));
        auto* tmplItem = new QTableWidgetItem(templateStr);
        auto* cntItem  = new QTableWidgetItem(
            QString::number(static_cast<qulonglong>(e.count)));
        auto* pctItem  = new QTableWidgetItem(
            QString("%1%").arg(pct, 0, 'f', 1));

        typeItem->setToolTip(QString::fromStdString(e.typeName));
        tmplItem->setToolTip(templateStr);
        cntItem->setTextAlignment(Qt::AlignCenter);
        pctItem->setTextAlignment(Qt::AlignCenter);

        m_templateTable->setItem(r, 0, typeItem);
        m_templateTable->setItem(r, 1, tmplItem);
        m_templateTable->setItem(r, 2, cntItem);
        m_templateTable->setItem(r, 3, pctItem);

        m_templateMatches.push_back(e.matching);
    }
}

// ============================================================================
// Tab 2 — Co-occurrence
// ============================================================================

namespace {

/// Attempt to parse a timestamp field to seconds-since-epoch.
/// Returns -1 if unparseable.
qint64 toEpochSecs(const std::string& s)
{
    if (s.empty()) return -1;
    const QString qs = QString::fromStdString(s);

    QDateTime dt = QDateTime::fromString(qs, Qt::ISODateWithMs);
    if (dt.isValid()) return dt.toSecsSinceEpoch();
    dt = QDateTime::fromString(qs, Qt::ISODate);
    if (dt.isValid()) return dt.toSecsSinceEpoch();
    for (const char* fmt : {"yyyy-MM-dd HH:mm:ss.zzz",
                            "yyyy-MM-dd HH:mm:ss"})
    {
        dt = QDateTime::fromString(qs, QString::fromLatin1(fmt));
        if (dt.isValid()) return dt.toSecsSinceEpoch();
    }
    bool ok = false;
    const qint64 epoch = qs.toLongLong(&ok);
    return ok ? epoch : -1;
}

} // anonymous namespace

void PatternAnalysisPanel::RefreshCooccurrence(
    const std::vector<unsigned long>& indices)
{
    m_coocTable->setRowCount(0);
    if (indices.size() < 2) return;

    const std::string& typeField = config::GetConfig().typeFilterField;
    const qint64 windowSecs = m_windowSpin->value();

    // Candidate timestamp field names (priority order)
    static const std::vector<std::string> kTsCandidates{
        "timestamp", "time", "datetime", "@timestamp", "date"};

    // Detect timestamp field
    std::string tsField;
    for (const auto& cand : kTsCandidates)
    {
        for (unsigned long idx : indices)
        {
            const std::string v =
                m_events.GetEvent(static_cast<int>(idx)).findByKey(cand);
            if (!v.empty() && toEpochSecs(v) >= 0)
            {
                tsField = cand;
                break;
            }
        }
        if (!tsField.empty()) break;
    }

    if (tsField.empty())
    {
        // No timestamp → show message
        m_coocTable->setRowCount(1);
        m_coocTable->setItem(0, 0,
            new QTableWidgetItem(tr("No parseable timestamp field found")));
        return;
    }

    // Collect (epoch_seconds, type) pairs
    struct TimedEvent
    {
        qint64      ts;
        std::string type;
    };
    std::vector<TimedEvent> events;
    events.reserve(indices.size());
    for (unsigned long idx : indices)
    {
        const auto& ev = m_events.GetEvent(static_cast<int>(idx));
        const qint64 ts = toEpochSecs(ev.findByKey(tsField));
        if (ts < 0) continue;
        const std::string t = ev.findByKey(typeField);
        events.push_back({ts, t.empty() ? "(none)" : t});
    }
    std::sort(events.begin(), events.end(),
              [](const TimedEvent& a, const TimedEvent& b) {
                  return a.ts < b.ts;
              });

    // Sliding window co-occurrence counting (unordered pair)
    // Limit to top pairs to avoid O(n²) explosion on huge logs
    constexpr size_t kMaxEvents = 20'000;
    const size_t n = std::min(events.size(), kMaxEvents);

    // Use canonical pair key: alphabetically sorted (A, B) with A ≤ B
    auto pairKey = [](const std::string& a, const std::string& b) -> std::string {
        return a <= b ? a + '\0' + b : b + '\0' + a;
    };

    std::unordered_map<std::string, int> coocCounts;

    for (size_t i = 0; i < n; ++i)
    {
        for (size_t j = i + 1; j < n; ++j)
        {
            if (events[j].ts - events[i].ts > windowSecs) break;
            if (events[i].type == events[j].type) continue;
            coocCounts[pairKey(events[i].type, events[j].type)]++;
        }
    }

    // Sort descending, top 50
    using Pair = std::pair<std::string, int>;
    std::vector<Pair> sorted(coocCounts.begin(), coocCounts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Pair& a, const Pair& b) { return a.second > b.second; });
    if (sorted.size() > 50) sorted.resize(50);

    m_coocTable->setRowCount(static_cast<int>(sorted.size()));
    for (int r = 0; r < static_cast<int>(sorted.size()); ++r)
    {
        const auto& [key, count] = sorted[static_cast<size_t>(r)];
        const size_t sep = key.find('\0');
        const QString typeA = QString::fromStdString(key.substr(0, sep));
        const QString typeB = sep != std::string::npos
            ? QString::fromStdString(key.substr(sep + 1))
            : QString();

        auto* aItem = new QTableWidgetItem(typeA);
        auto* bItem = new QTableWidgetItem(typeB);
        auto* cItem = new QTableWidgetItem(QString::number(count));
        aItem->setToolTip(typeA);
        bItem->setToolTip(typeB);
        cItem->setTextAlignment(Qt::AlignCenter);
        m_coocTable->setItem(r, 0, aItem);
        m_coocTable->setItem(r, 1, bItem);
        m_coocTable->setItem(r, 2, cItem);
    }
}

// ============================================================================
// Tab 3 — Sequence N-grams
// ============================================================================

void PatternAnalysisPanel::RefreshNgrams(
    const std::vector<unsigned long>& indices)
{
    m_ngramTable->setRowCount(0);
    if (indices.empty()) return;

    const std::string& typeField = config::GetConfig().typeFilterField;
    const int n    = m_ngramSizeCombo->currentData().toInt();
    const int topN = m_ngramTopNSpin->value();

    // Extract the event-type sequence in original (visible) order
    std::vector<std::string> typeSeq;
    typeSeq.reserve(indices.size());
    for (unsigned long idx : indices)
    {
        const std::string t =
            m_events.GetEvent(static_cast<int>(idx)).findByKey(typeField);
        typeSeq.push_back(t.empty() ? "(none)" : t);
    }

    // Count n-grams
    const size_t total = typeSeq.size() > static_cast<size_t>(n)
        ? typeSeq.size() - static_cast<size_t>(n) + 1
        : 0;

    std::map<std::string, size_t> counts;
    for (size_t i = 0; i < total; ++i)
    {
        // Build key from n consecutive types joined by " → "
        std::string key;
        for (int k = 0; k < n; ++k)
        {
            if (k) key += " \xe2\x86\x92 "; // UTF-8 →
            key += typeSeq[i + static_cast<size_t>(k)];
        }
        counts[key]++;
    }

    // Sort descending
    using Pair = std::pair<std::string, size_t>;
    std::vector<Pair> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Pair& a, const Pair& b) { return a.second > b.second; });
    if (static_cast<int>(sorted.size()) > topN)
        sorted.resize(static_cast<size_t>(topN));

    m_ngramTable->setRowCount(static_cast<int>(sorted.size()));
    for (int r = 0; r < static_cast<int>(sorted.size()); ++r)
    {
        const auto& [key, count] = sorted[static_cast<size_t>(r)];
        const double pct =
            total > 0 ? 100.0 * count / static_cast<double>(total) : 0.0;

        const QString qKey = QString::fromStdString(key);
        auto* seqItem = new QTableWidgetItem(qKey);
        auto* cntItem = new QTableWidgetItem(
            QString::number(static_cast<qulonglong>(count)));
        auto* pctItem = new QTableWidgetItem(
            QString("%1%").arg(pct, 0, 'f', 1));

        seqItem->setToolTip(qKey);
        cntItem->setTextAlignment(Qt::AlignCenter);
        pctItem->setTextAlignment(Qt::AlignCenter);

        m_ngramTable->setItem(r, 0, seqItem);
        m_ngramTable->setItem(r, 1, cntItem);
        m_ngramTable->setItem(r, 2, pctItem);
    }
}

} // namespace ui::qt
