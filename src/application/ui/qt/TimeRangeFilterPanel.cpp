/**
 * @file TimeRangeFilterPanel.cpp
 * @brief Implementation of TimeRangeFilterPanel.
 * @author LogViewer Development Team
 * @date 2026
 */
#include "TimeRangeFilterPanel.hpp"

#include "EventsContainer.hpp"
#include "EventsTableView.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ui::qt {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TimeRangeFilterPanel::TimeRangeFilterPanel(db::EventsContainer& events,
                                           EventsTableView*     eventsView,
                                           QWidget*             parent)
    : QWidget(parent), m_events(events), m_eventsView(eventsView)
{
    BuildLayout();
}

void TimeRangeFilterPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* infoLabel = new QLabel(
        tr("Filter events by timestamp field value.\n"
           "From / To bounds are inclusive string comparisons.\n"
           "Leave a bound empty to make it open-ended."),
        this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // ── Field selector ────────────────────────────────────────────────────
    auto* form = new QFormLayout();

    m_fieldCombo = new QComboBox(this);
    m_fieldCombo->setEditable(true);
    m_fieldCombo->setInsertPolicy(QComboBox::NoInsert);
    m_fieldCombo->addItems(
        {"timestamp", "time", "datetime", "@timestamp", "date", "ts"});
    m_fieldCombo->setToolTip(tr("Name of the timestamp field in the log events"));
    form->addRow(tr("Timestamp field:"), m_fieldCombo);

    // ── From / To ─────────────────────────────────────────────────────────
    m_fromEdit = new QLineEdit(this);
    m_fromEdit->setPlaceholderText(tr("e.g. 2025-01-01T08:00:00"));
    m_fromEdit->setToolTip(tr("Inclusive lower bound (empty = no limit)"));
    form->addRow(tr("From:"), m_fromEdit);

    m_toEdit = new QLineEdit(this);
    m_toEdit->setPlaceholderText(tr("e.g. 2025-01-01T18:00:00"));
    m_toEdit->setToolTip(tr("Inclusive upper bound (empty = no limit)"));
    form->addRow(tr("To:"), m_toEdit);

    layout->addLayout(form);

    // ── Auto-detect button ────────────────────────────────────────────────
    auto* detectBtn = new QPushButton(tr("Auto-detect range from data"), this);
    detectBtn->setToolTip(
        tr("Scan all events and fill From / To with the earliest and latest "
           "timestamps found in the selected field"));
    layout->addWidget(detectBtn);

    // ── Apply / Clear ─────────────────────────────────────────────────────
    auto* btnRow    = new QHBoxLayout();
    auto* applyBtn  = new QPushButton(tr("Apply Filter"), this);
    auto* clearBtn  = new QPushButton(tr("Clear Filter"), this);
    applyBtn->setDefault(false);
    btnRow->addWidget(applyBtn);
    btnRow->addWidget(clearBtn);
    layout->addLayout(btnRow);

    // ── Status ────────────────────────────────────────────────────────────
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("font-style: italic;");
    layout->addWidget(m_statusLabel);

    layout->addStretch();

    connect(detectBtn, &QPushButton::clicked, this, &TimeRangeFilterPanel::HandleAutoDetect);
    connect(applyBtn,  &QPushButton::clicked, this, &TimeRangeFilterPanel::HandleApply);
    connect(clearBtn,  &QPushButton::clicked, this, &TimeRangeFilterPanel::HandleClear);
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

TimeRangeFilterPanel::State TimeRangeFilterPanel::GetState() const
{
    State s;
    s.field  = m_fieldCombo->currentText().trimmed().toStdString();
    s.from   = m_fromEdit->text().trimmed().toStdString();
    s.to     = m_toEdit->text().trimmed().toStdString();
    s.active = m_active;
    return s;
}

void TimeRangeFilterPanel::SetState(const State& s)
{
    const int idx = m_fieldCombo->findText(QString::fromStdString(s.field));
    if (idx >= 0)
        m_fieldCombo->setCurrentIndex(idx);
    else
        m_fieldCombo->setEditText(QString::fromStdString(s.field));

    m_fromEdit->setText(QString::fromStdString(s.from));
    m_toEdit->setText(QString::fromStdString(s.to));
    m_active = s.active;
}

void TimeRangeFilterPanel::Apply()
{
    HandleApply();
}

void TimeRangeFilterPanel::Clear()
{
    HandleClear();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void TimeRangeFilterPanel::HandleAutoDetect()
{
    const std::string field =
        m_fieldCombo->currentText().trimmed().toStdString();
    if (field.empty())
    {
        m_statusLabel->setText(tr("Enter a field name first."));
        return;
    }

    std::string minTs, maxTs;
    const size_t total = m_events.Size();
    for (size_t i = 0; i < total; ++i)
    {
        const std::string ts =
            m_events.GetEvent(static_cast<int>(i)).findByKey(field);
        if (ts.empty()) continue;
        if (minTs.empty() || ts < minTs) minTs = ts;
        if (maxTs.empty() || ts > maxTs) maxTs = ts;
    }

    if (minTs.empty())
    {
        m_statusLabel->setText(
            tr("No events contain field \"%1\".")
                .arg(QString::fromStdString(field)));
        return;
    }

    m_fromEdit->setText(QString::fromStdString(minTs));
    m_toEdit->setText(QString::fromStdString(maxTs));
    m_statusLabel->setText(
        tr("Range detected from %1 events.")
            .arg(total));
}

void TimeRangeFilterPanel::HandleApply()
{
    if (!m_eventsView) return;

    const std::string from = m_fromEdit->text().trimmed().toStdString();
    const std::string to   = m_toEdit->text().trimmed().toStdString();

    if (from.empty() && to.empty())
    {
        m_statusLabel->setText(tr("Specify at least one bound (From or To)."));
        return;
    }

    const auto indices = ComputeMatchingIndices();

    if (indices.empty())
    {
        m_statusLabel->setText(tr("No events match the specified range."));
        m_eventsView->SetFilteredEvents(indices);
        m_active = true;
        emit FilterApplied();
        return;
    }

    m_eventsView->SetFilteredEvents(indices);
    m_active = true;
    m_statusLabel->setText(
        tr("Filter active: %1 event(s) in range.").arg(indices.size()));
    emit FilterApplied();
}

void TimeRangeFilterPanel::HandleClear()
{
    if (!m_eventsView) return;
    m_eventsView->ClearFilter();
    m_active = false;
    m_statusLabel->clear();
    emit FilterCleared();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::vector<unsigned long> TimeRangeFilterPanel::ComputeMatchingIndices() const
{
    const std::string field =
        m_fieldCombo->currentText().trimmed().toStdString();
    const std::string from = m_fromEdit->text().trimmed().toStdString();
    const std::string to   = m_toEdit->text().trimmed().toStdString();

    std::vector<unsigned long> result;
    const size_t total = m_events.Size();
    result.reserve(total);

    for (size_t i = 0; i < total; ++i)
    {
        const std::string ts =
            m_events.GetEvent(static_cast<int>(i)).findByKey(field);

        // Events with no timestamp in the specified field are excluded when
        // any bound is set, because their position in the timeline is unknown.
        if (ts.empty()) continue;

        if (!from.empty() && ts < from) continue;
        if (!to.empty()   && ts > to)   continue;

        result.push_back(static_cast<unsigned long>(i));
    }

    return result;
}

} // namespace ui::qt
