#include "DashboardPanel.hpp"
#include "EventsContainer.hpp"
#include "Config.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QWidget>
#include <QFont>
#include <QDateTime>
#include <QFileInfo>
#include <map>

namespace ui::qt {

DashboardPanel::DashboardPanel(QWidget* parent)
    : QWidget(parent)
{
    CreateLayout();
}

void DashboardPanel::CreateLayout()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // Scroll area for all content
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet("QScrollArea { background-color: transparent; border: none; }");

    auto* scrollWidget = new QWidget();
    auto* scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(12);

    // ═══════════════════════════════════════════════════════════════════════
    // Title
    // ═══════════════════════════════════════════════════════════════════════
    auto* titleLabel = new QLabel("📊 Log Analysis Dashboard");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    scrollLayout->addWidget(titleLabel);

    // ═══════════════════════════════════════════════════════════════════════
    // File Information Section
    // ═══════════════════════════════════════════════════════════════════════
    auto* fileInfoGroup = new QGroupBox("📁 File Information");
    auto* fileInfoLayout = new QVBoxLayout(fileInfoGroup);

    auto* fileNameRow = new QHBoxLayout();
    fileNameRow->addWidget(new QLabel("Name:"));
    m_fileNameLabel = new QLabel("(No file loaded)");
    m_fileNameLabel->setStyleSheet("font-weight: bold;");
    fileNameRow->addWidget(m_fileNameLabel);
    fileNameRow->addStretch();
    fileInfoLayout->addLayout(fileNameRow);

    auto* fileFormatRow = new QHBoxLayout();
    fileFormatRow->addWidget(new QLabel("Format:"));
    m_fileFormatLabel = new QLabel("—");
    fileFormatRow->addWidget(m_fileFormatLabel);
    fileFormatRow->addStretch();
    fileInfoLayout->addLayout(fileFormatRow);

    auto* fileSizeRow = new QHBoxLayout();
    fileSizeRow->addWidget(new QLabel("Size:"));
    m_fileSizeLabel = new QLabel("—");
    fileSizeRow->addWidget(m_fileSizeLabel);
    fileSizeRow->addStretch();
    fileInfoLayout->addLayout(fileSizeRow);

    auto* timeRangeRow = new QHBoxLayout();
    timeRangeRow->addWidget(new QLabel("Time Range:"));
    m_timeRangeLabel = new QLabel("—");
    timeRangeRow->addWidget(m_timeRangeLabel);
    timeRangeRow->addStretch();
    fileInfoLayout->addLayout(timeRangeRow);

    scrollLayout->addWidget(fileInfoGroup);

    // ═══════════════════════════════════════════════════════════════════════
    // Statistics Section
    // ═══════════════════════════════════════════════════════════════════════
    auto* statsGroup = new QGroupBox("📈 Event Statistics");
    auto* statsLayout = new QVBoxLayout(statsGroup);

    auto* totalRow = new QHBoxLayout();
    totalRow->addWidget(new QLabel("Total Events:"));
    m_totalEventsLabel = new QLabel("0");
    QFont boldFont = m_totalEventsLabel->font();
    boldFont.setBold(true);
    m_totalEventsLabel->setFont(boldFont);
    totalRow->addWidget(m_totalEventsLabel);
    totalRow->addStretch();
    statsLayout->addLayout(totalRow);

    m_typeBreakdownTitleLabel = new QLabel("Breakdown:");
    m_typeBreakdownTitleLabel->setStyleSheet("font-weight: bold;");
    statsLayout->addWidget(m_typeBreakdownTitleLabel);

    m_typeBreakdownLabel = new QLabel("(No data yet)");
    m_typeBreakdownLabel->setObjectName("dashboardTypeBreakdownLabel");
    m_typeBreakdownLabel->setWordWrap(true);
    statsLayout->addWidget(m_typeBreakdownLabel);

    scrollLayout->addWidget(statsGroup);

    // ═══════════════════════════════════════════════════════════════════════
    // Top Actors Section
    // ═══════════════════════════════════════════════════════════════════════
    auto* actorsGroup = new QGroupBox("🎭 Top Actors");
    auto* actorsLayout = new QVBoxLayout(actorsGroup);
    m_topActorsLabel = new QLabel("(No actors yet)");
    m_topActorsLabel->setWordWrap(true);
    actorsLayout->addWidget(m_topActorsLabel);
    scrollLayout->addWidget(actorsGroup);

    // ═══════════════════════════════════════════════════════════════════════
    // Quick Actions Section
    // ═══════════════════════════════════════════════════════════════════════
    auto* actionsGroup = new QGroupBox("⚡ Quick Actions");
    auto* actionsLayout = new QVBoxLayout(actionsGroup);

    auto* buttonRow = new QHBoxLayout();
    m_exportButton = new QPushButton("📤 Export");
    m_reportButton = new QPushButton("📋 Generate Report");
    m_bookmarkButton = new QPushButton("🏷️ Bookmark");

    connect(m_exportButton, &QPushButton::clicked, this, &DashboardPanel::OnExportClicked);
    connect(m_reportButton, &QPushButton::clicked, this, &DashboardPanel::OnReportClicked);
    connect(m_bookmarkButton, &QPushButton::clicked, this, &DashboardPanel::OnBookmarkClicked);

    buttonRow->addWidget(m_exportButton);
    buttonRow->addWidget(m_reportButton);
    buttonRow->addWidget(m_bookmarkButton);
    buttonRow->addStretch();

    actionsLayout->addLayout(buttonRow);
    scrollLayout->addWidget(actionsGroup);

    scrollLayout->addStretch();
    m_scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(m_scrollArea);
}

void DashboardPanel::SetEventsSource(db::EventsContainer* events)
{
    m_events = events;

    if (m_events)
    {
        // Update stats when events source is set
        UpdateStats();
    }
}

void DashboardPanel::UpdateStats()
{
    if (!m_events)
        return;

    UpdateFileInfo();
    UpdateEventStats();
    UpdateTopActors();
}

void DashboardPanel::UpdateFileInfo()
{
    if (!m_events)
        return;

    // File info is typically stored in EventsContainer
    // For now, show basic stats
    m_fileNameLabel->setText("(Log file)");
    m_fileFormatLabel->setText("—");
    m_fileSizeLabel->setText("—");
    m_timeRangeLabel->setText("—");
}

void DashboardPanel::UpdateEventStats()
{
    const std::string& typeField = config::GetConfig().typeFilterField;
    const QString typeFieldLabel = typeField.empty()
        ? tr("type")
        : QString::fromStdString(typeField);
    m_typeBreakdownTitleLabel->setText(tr("Breakdown by \"%1\":").arg(typeFieldLabel));

    if (!m_events || m_events->Size() == 0)
    {
        m_totalEventsLabel->setText("0");
        m_typeBreakdownLabel->setText(tr("(No data yet)"));
        return;
    }

    qint64 totalCount = m_events->Size();
    std::map<QString, qint64> valueCounts;

    // Count events by the configured type field (thread-safe: cache size first)
    const size_t eventCount = m_events->Size();
    for (size_t i = 0; i < eventCount; ++i)
    {
        try
        {
            // Gracefully handle concurrent modifications
            if (i >= m_events->Size())
                break;

            const auto& event = m_events->GetEvent(i);
            QString value = QString::fromStdString(event.findByKey(typeField));
            if (!value.isEmpty())
                valueCounts[value]++;
        }
        catch (const std::exception&)
        {
            // Skip events with errors (removed, invalid, etc.)
        }
    }

    m_totalEventsLabel->setText(FormatNumber(totalCount));

    if (valueCounts.empty())
    {
        m_typeBreakdownLabel->setText(typeField.empty()
            ? tr("(No type filter field configured)")
            : tr("(No events have a \"%1\" field)").arg(typeFieldLabel));
        return;
    }

    // Sort by count descending, show top 8
    std::vector<std::pair<QString, qint64>> sorted(valueCounts.begin(), valueCounts.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    QString breakdownText;
    int shown = 0;
    for (const auto& [value, count] : sorted)
    {
        if (shown >= 8)
            break;
        breakdownText += QString("• %1: %2\n").arg(value, FormatNumber(count));
        ++shown;
    }
    if (sorted.size() > 8)
        breakdownText += tr("… and %1 more").arg(sorted.size() - 8);

    m_typeBreakdownLabel->setText(breakdownText.trimmed());
}

void DashboardPanel::UpdateTopActors()
{
    if (!m_events || m_events->Size() == 0)
    {
        m_topActorsLabel->setText("(No actors yet)");
        return;
    }

    std::map<QString, qint64> actorCounts;

    // Count events by actor (thread-safe: cache size first)
    const size_t eventCount = m_events->Size();
    for (size_t i = 0; i < eventCount; ++i)
    {
        try
        {
            // Gracefully handle concurrent modifications
            if (i >= m_events->Size())
                break;

            const auto& event = m_events->GetEvent(i);
            QString actor = QString::fromStdString(event.findByKey("actor"));
            if (!actor.isEmpty())
                actorCounts[actor]++;
        }
        catch (const std::exception&)
        {
            // Skip events with errors (removed, invalid, etc.)
        }
    }

    if (actorCounts.empty())
    {
        m_topActorsLabel->setText("(No actors in log)");
        return;
    }

    // Sort by count descending
    std::vector<std::pair<QString, qint64>> sorted(actorCounts.begin(), actorCounts.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Show top 5
    QString topActorsText;
    int count = 0;
    for (const auto& [actor, cnt] : sorted)
    {
        if (count >= 5)
            break;
        topActorsText += QString("• %1 (%2 events)\n").arg(actor).arg(FormatNumber(cnt));
        count++;
    }

    m_topActorsLabel->setText(topActorsText.trimmed());
}

QString DashboardPanel::FormatNumber(qint64 count)
{
    if (count >= 1'000'000)
        return QString::number(count / 1'000'000.0, 'f', 1) + "M";
    if (count >= 1'000)
        return QString::number(count / 1'000.0, 'f', 1) + "K";
    return QString::number(count);
}

void DashboardPanel::OnExportClicked()
{
    emit ExportRequested();
}

void DashboardPanel::OnReportClicked()
{
    emit GenerateReportRequested();
}

void DashboardPanel::OnBookmarkClicked()
{
    emit BookmarkCurrentRequested();
}

void DashboardPanel::OnEventsChanged()
{
    UpdateStats();
}

} // namespace ui::qt
