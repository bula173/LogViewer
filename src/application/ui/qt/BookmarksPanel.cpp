#include "BookmarksPanel.hpp"

#include "EventsContainer.hpp"
#include "EventsTableView.hpp"

#include <QGuiApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <string>

namespace ui::qt {

BookmarksPanel::BookmarksPanel(db::EventsContainer& events,
                               EventsTableView*     eventsView,
                               QWidget*             parent)
    : QWidget(parent), m_events(events), m_eventsView(eventsView)
{
    BuildLayout();
    RebuildTable();

    connect(m_eventsView, &EventsTableView::CurrentActualRowChanged,
            this, &BookmarksPanel::OnCurrentRowChanged);
}

void BookmarksPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // ── Add-bookmark row ──────────────────────────────────────────────────
    auto* addRow = new QHBoxLayout();
    addRow->addWidget(new QLabel(tr("Label:"), this));
    m_labelEdit = new QLineEdit(this);
    m_labelEdit->setPlaceholderText(tr("Optional annotation…"));
    m_labelEdit->setToolTip(
        tr("Optional label for the bookmark — press Enter or click Bookmark"));
    addRow->addWidget(m_labelEdit, 1);
    m_addBtn = new QPushButton(tr("Bookmark"), this);
    m_addBtn->setToolTip(tr("Bookmark the currently selected event"));
    m_addBtn->setEnabled(false);
    addRow->addWidget(m_addBtn);
    layout->addLayout(addRow);

    // ── Bookmarks table ───────────────────────────────────────────────────
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(
        {tr("Row"), tr("Label"), tr("Timestamp"), tr("Summary")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->setColumnWidth(2, 160);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table, 1);

    // ── Action buttons ────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    m_goToBtn = new QPushButton(tr("Go To"), this);
    m_goToBtn->setToolTip(
        tr("Scroll to and select the bookmarked event.\n"
           "If the event is currently filtered out, clear the filter first."));
    m_goToBtn->setEnabled(false);
    m_removeBtn = new QPushButton(tr("Remove"), this);
    m_removeBtn->setToolTip(tr("Remove the selected bookmark"));
    m_removeBtn->setEnabled(false);
    m_exportBtn = new QPushButton(tr("Export as Markdown"), this);
    m_exportBtn->setToolTip(tr("Copy all bookmarks as a Markdown table to the clipboard"));
    btnRow->addWidget(m_goToBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_exportBtn);
    layout->addLayout(btnRow);

    // ── Status label ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("No bookmarks"), this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_statusLabel);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_addBtn,    &QPushButton::clicked, this, &BookmarksPanel::OnAddBookmark);
    connect(m_labelEdit, &QLineEdit::returnPressed, this, &BookmarksPanel::OnAddBookmark);
    connect(m_goToBtn,   &QPushButton::clicked, this, &BookmarksPanel::OnGoTo);
    connect(m_removeBtn, &QPushButton::clicked, this, &BookmarksPanel::OnRemove);
    connect(m_exportBtn, &QPushButton::clicked, this, &BookmarksPanel::OnExport);

    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        const bool sel = !m_table->selectedItems().isEmpty();
        m_goToBtn->setEnabled(sel);
        m_removeBtn->setEnabled(sel);
    });

    connect(m_table, &QTableWidget::itemDoubleClicked,
            this, [this](QTableWidgetItem*) { OnGoTo(); });
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void BookmarksPanel::OnCurrentRowChanged(int actualRow)
{
    m_currentRow = actualRow;
    m_addBtn->setEnabled(actualRow >= 0);
}

void BookmarksPanel::AddBookmarkForRow(int actualRow)
{
    DoAddBookmark(actualRow, {});
}

void BookmarksPanel::OnAddBookmark()
{
    DoAddBookmark(m_currentRow, m_labelEdit->text().toStdString());
    m_labelEdit->clear();
}

void BookmarksPanel::DoAddBookmark(int actualRow, const std::string& label)
{
    if (actualRow < 0 || actualRow >= static_cast<int>(m_events.Size()))
        return;

    const db::LogEvent& ev = m_events.GetEvent(actualRow);

    static const std::vector<std::string> kMsgFields{
        "message", "msg", "text", "description", "body"};
    static const std::vector<std::string> kTsFields{
        "timestamp", "time", "datetime", "@timestamp", "date"};

    std::string summary;
    for (const auto& f : kMsgFields)
    {
        summary = ev.findByKey(f);
        if (!summary.empty()) break;
    }
    if (summary.empty())
    {
        const auto& items = ev.getEventItems();
        if (!items.empty()) summary = items.front().second;
    }
    if (summary.size() > 80)
        summary = summary.substr(0, 77) + "...";

    std::string ts;
    for (const auto& f : kTsFields)
    {
        ts = ev.findByKey(f);
        if (!ts.empty()) break;
    }

    m_bookmarks.push_back({actualRow, label, summary, ts});
    RebuildTable();
    m_statusLabel->setText(tr("Added bookmark for event #%1").arg(actualRow));
}

void BookmarksPanel::OnGoTo()
{
    const int sel = SelectedTableRow();
    if (sel < 0 || sel >= static_cast<int>(m_bookmarks.size())) return;
    emit NavigateToEvent(m_bookmarks[static_cast<size_t>(sel)].row);
}

void BookmarksPanel::OnRemove()
{
    const int sel = SelectedTableRow();
    if (sel < 0 || sel >= static_cast<int>(m_bookmarks.size())) return;
    m_bookmarks.erase(m_bookmarks.begin() + sel);
    RebuildTable();
    m_statusLabel->setText(tr("%1 bookmark(s)").arg(m_bookmarks.size()));
}

void BookmarksPanel::OnExport()
{
    if (m_bookmarks.empty())
    {
        m_statusLabel->setText(tr("No bookmarks to export"));
        return;
    }

    auto escape = [](const std::string& s) -> QString {
        QString q = QString::fromStdString(s);
        q.replace('|', "\\|");
        q.replace('\n', ' ');
        return q;
    };

    QString md = "# Log Bookmarks\n\n";
    md += "| # | Row | Label | Timestamp | Summary |\n";
    md += "|---|-----|-------|-----------|--------|\n";
    for (size_t i = 0; i < m_bookmarks.size(); ++i)
    {
        const auto& bm = m_bookmarks[i];
        md += QString("| %1 | %2 | %3 | %4 | %5 |\n")
                  .arg(i + 1)
                  .arg(bm.row)
                  .arg(escape(bm.label))
                  .arg(escape(bm.timestamp))
                  .arg(escape(bm.summary));
    }

    QGuiApplication::clipboard()->setText(md);
    m_statusLabel->setText(
        tr("Copied %1 bookmark(s) to clipboard as Markdown").arg(m_bookmarks.size()));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void BookmarksPanel::RebuildTable()
{
    m_table->setRowCount(static_cast<int>(m_bookmarks.size()));
    for (int i = 0; i < static_cast<int>(m_bookmarks.size()); ++i)
    {
        const auto& bm = m_bookmarks[static_cast<size_t>(i)];
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(bm.row)));
        m_table->setItem(i, 1,
            new QTableWidgetItem(QString::fromStdString(bm.label)));
        m_table->setItem(i, 2,
            new QTableWidgetItem(QString::fromStdString(bm.timestamp)));
        m_table->setItem(i, 3,
            new QTableWidgetItem(QString::fromStdString(bm.summary)));
    }
    m_goToBtn->setEnabled(false);
    m_removeBtn->setEnabled(false);
    m_statusLabel->setText(m_bookmarks.empty()
        ? tr("No bookmarks")
        : tr("%1 bookmark(s)").arg(m_bookmarks.size()));
}

nlohmann::json BookmarksPanel::GetSessionData() const
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& bm : m_bookmarks)
        arr.push_back({{"row", bm.row}, {"label", bm.label},
                       {"timestamp", bm.timestamp}, {"summary", bm.summary}});
    return arr;
}

void BookmarksPanel::LoadSessionData(const nlohmann::json& data)
{
    m_bookmarks.clear();
    for (const auto& item : data)
    {
        Bookmark bm;
        bm.row       = item.value("row",       -1);
        bm.label     = item.value("label",     std::string{});
        bm.timestamp = item.value("timestamp", std::string{});
        bm.summary   = item.value("summary",   std::string{});
        if (bm.row >= 0)
            m_bookmarks.push_back(std::move(bm));
    }
    RebuildTable();
}

int BookmarksPanel::SelectedTableRow() const
{
    const auto sel = m_table->selectedItems();
    if (sel.isEmpty()) return -1;
    return m_table->row(sel.first());
}

} // namespace ui::qt
