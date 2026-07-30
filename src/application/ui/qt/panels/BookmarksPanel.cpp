#include "BookmarksPanel.hpp"

#include "Logger.hpp"
#include "utils/PanelUtils.hpp"

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
#include <QComboBox>
#include <QFileDialog>
#include <QInputDialog>

#include <algorithm>
#include <string>
#include <fstream>
#include <filesystem>

namespace ui::qt {

BookmarksPanel::BookmarksPanel(db::EventsContainer& events,
                               EventsTableView*     eventsView,
                               QWidget*             parent)
    : QWidget(parent), m_events(events), m_eventsView(eventsView)
{
    m_categories = GetDefaultCategories();
    BuildLayout();
    RebuildTable();

    connect(m_eventsView, &EventsTableView::CurrentActualRowChanged,
            this, &BookmarksPanel::OnCurrentRowChanged);
}

std::vector<BookmarksPanel::Category> BookmarksPanel::GetDefaultCategories()
{
    return {
        {"Bug", "#FF6B6B"},          // Red
        {"TODO", "#4ECDC4"},         // Teal
        {"Performance", "#FFE66D"},  // Yellow
        {"Important", "#FF6348"},    // Orange-red
        {"Question", "#95E1D3"},     // Mint
        {"Note", "#A8E6CF"}          // Light green
    };
}

void BookmarksPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // ── Category management row ───────────────────────────────────────────
    auto* catRow = new QHBoxLayout();
    catRow->addWidget(new QLabel(tr("Category:"), this));
    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->setToolTip(tr("Select category for new bookmarks"));
    for (const auto& cat : m_categories)
    {
        m_categoryCombo->addItem(QString::fromStdString(cat.name));
    }
    catRow->addWidget(m_categoryCombo);
    m_addCategoryBtn = new QPushButton(tr("Add"), this);
    m_addCategoryBtn->setToolTip(tr("Add a new category"));
    m_addCategoryBtn->setMaximumWidth(50);
    catRow->addWidget(m_addCategoryBtn);
    m_removeCategoryBtn = new QPushButton(tr("Remove"), this);
    m_removeCategoryBtn->setToolTip(tr("Remove selected category"));
    m_removeCategoryBtn->setMaximumWidth(70);
    catRow->addWidget(m_removeCategoryBtn);
    catRow->addStretch();
    layout->addLayout(catRow);

    // ── Add-bookmark row ──────────────────────────────────────────────────
    auto* addRow = new QHBoxLayout();
    addRow->addWidget(new QLabel(tr("Label:"), this));
    m_labelEdit = new QLineEdit(this);
    m_labelEdit->setPlaceholderText(tr("Optional annotation…"));
    m_labelEdit->setToolTip(tr("Optional label for the bookmark"));
    addRow->addWidget(m_labelEdit, 1);
    m_addBtn = new QPushButton(tr("Bookmark"), this);
    m_addBtn->setToolTip(tr("Bookmark the currently selected event"));
    m_addBtn->setEnabled(false);
    addRow->addWidget(m_addBtn);
    layout->addLayout(addRow);

    // ── Search and filter row ─────────────────────────────────────────────
    auto* filterRow = new QHBoxLayout();
    filterRow->addWidget(new QLabel(tr("Filter:"), this));
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search bookmarks…"));
    m_searchEdit->setToolTip(tr("Filter bookmarks by keyword"));
    filterRow->addWidget(m_searchEdit, 1);
    filterRow->addWidget(new QLabel(tr("Category:"), this));
    m_categoryFilterCombo = new QComboBox(this);
    m_categoryFilterCombo->addItem(tr("All"));
    for (const auto& cat : m_categories)
    {
        m_categoryFilterCombo->addItem(QString::fromStdString(cat.name));
    }
    m_categoryFilterCombo->setToolTip(tr("Filter by category"));
    m_categoryFilterCombo->setMaximumWidth(150);
    filterRow->addWidget(m_categoryFilterCombo);
    layout->addLayout(filterRow);

    // ── Bookmarks table ───────────────────────────────────────────────────
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels(
        {tr("Row"), tr("Category"), tr("Label"), tr("Timestamp"), tr("Summary")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_table->setColumnWidth(3, 160);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table, 1);

    // ── Action buttons ────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    m_goToBtn = new QPushButton(tr("Go To"), this);
    m_goToBtn->setToolTip(tr("Scroll to and select the bookmarked event"));
    m_goToBtn->setEnabled(false);
    m_removeBtn = new QPushButton(tr("Remove"), this);
    m_removeBtn->setToolTip(tr("Remove the selected bookmark"));
    m_removeBtn->setEnabled(false);
    m_exportBtn = new QPushButton(tr("Export"), this);
    m_exportBtn->setToolTip(tr("Export bookmarks as JSON"));
    m_importBtn = new QPushButton(tr("Import"), this);
    m_importBtn->setToolTip(tr("Import bookmarks from JSON"));
    btnRow->addWidget(m_goToBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_importBtn);
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
    connect(m_importBtn, &QPushButton::clicked, this, &BookmarksPanel::OnImport);
    connect(m_addCategoryBtn, &QPushButton::clicked, this, &BookmarksPanel::OnAddCategory);
    connect(m_removeCategoryBtn, &QPushButton::clicked, this, &BookmarksPanel::OnRemoveCategory);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &BookmarksPanel::OnSearchTextChanged);
    connect(m_categoryFilterCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &BookmarksPanel::OnCategoryFilterChanged);

    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        const bool sel = !m_table->selectedItems().isEmpty();
        m_goToBtn->setEnabled(sel);
        m_removeBtn->setEnabled(sel);
    });

    connect(m_table, &QTableWidget::itemDoubleClicked,
            this, [this](QTableWidgetItem*) { OnGoTo(); });
}

void BookmarksPanel::OnCurrentRowChanged(int actualRow)
{
    m_currentRow = actualRow;
    m_addBtn->setEnabled(actualRow >= 0);
}

void BookmarksPanel::AddBookmarkForRow(int actualRow)
{
    DoAddBookmark(actualRow, {}, m_categoryCombo->currentText().toStdString());
}

void BookmarksPanel::OnAddBookmark()
{
    DoAddBookmark(m_currentRow, m_labelEdit->text().toStdString(),
                  m_categoryCombo->currentText().toStdString());
    m_labelEdit->clear();
}

void BookmarksPanel::DoAddBookmark(int actualRow, const std::string& label, const std::string& category)
{
    if (actualRow < 0 || actualRow >= static_cast<int>(m_events.Size()))
    {
        util::Logger::Warn("[BookmarksPanel] Invalid bookmark row {}", actualRow);
        return;
    }

    util::Logger::Debug("[BookmarksPanel] Adding bookmark at row {} with category '{}'", actualRow, category);

    const db::LogEvent& ev = m_events.GetEvent(static_cast<size_t>(actualRow));

    std::string summary;
    for (const auto& f : panel_utils::kMsgFields)
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
    for (const auto& f : panel_utils::kTsFields)
    {
        ts = ev.findByKey(f);
        if (!ts.empty()) break;
    }

    m_bookmarks.push_back({actualRow, label, category, summary, ts});
    util::Logger::Info("[BookmarksPanel] {} bookmark(s) total", m_bookmarks.size());
    UpdateCategoryCounts();
    ApplyFilters();
    m_statusLabel->setText(tr("Added bookmark for event #%1").arg(actualRow));
}

void BookmarksPanel::OnGoTo()
{
    const int sel = SelectedTableRow();
    if (sel < 0 || sel >= static_cast<int>(m_filteredBookmarks.size())) return;
    util::Logger::Debug("[BookmarksPanel] Navigating to bookmarked event row {}", m_filteredBookmarks[static_cast<size_t>(sel)].row);
    emit NavigateToEvent(m_filteredBookmarks[static_cast<size_t>(sel)].row);
}

void BookmarksPanel::OnRemove()
{
    const int sel = SelectedTableRow();
    if (sel < 0 || sel >= static_cast<int>(m_filteredBookmarks.size())) return;

    const auto& filtered = m_filteredBookmarks[static_cast<size_t>(sel)];
    auto it = std::find_if(m_bookmarks.begin(), m_bookmarks.end(),
        [&filtered](const Bookmark& bm) { return bm.row == filtered.row && bm.label == filtered.label; });
    if (it != m_bookmarks.end())
    {
        util::Logger::Debug("[BookmarksPanel] Removing bookmark at index");
        m_bookmarks.erase(it);
    }

    UpdateCategoryCounts();
    ApplyFilters();
    m_statusLabel->setText(tr("%1 bookmark(s)").arg(m_bookmarks.size()));
}

void BookmarksPanel::OnExport()
{
    if (m_bookmarks.empty())
    {
        util::Logger::Warn("[BookmarksPanel] Export requested but no bookmarks available");
        m_statusLabel->setText(tr("No bookmarks to export"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Bookmarks"),
        QString(), tr("JSON Files (*.json);;Markdown Files (*.md)"));
    if (fileName.isEmpty()) return;

    try
    {
        if (fileName.endsWith(".json", Qt::CaseInsensitive))
        {
            nlohmann::json j;
            j["version"] = "1.0";
            j["categories"] = nlohmann::json::array();
            for (const auto& cat : m_categories)
            {
                j["categories"].push_back({{"name", cat.name}, {"color", cat.color}});
            }
            j["bookmarks"] = nlohmann::json::array();
            for (const auto& bm : m_bookmarks)
            {
                j["bookmarks"].push_back({
                    {"row", bm.row},
                    {"label", bm.label},
                    {"category", bm.category},
                    {"timestamp", bm.timestamp},
                    {"summary", bm.summary}
                });
            }

            std::ofstream file(std::filesystem::path(fileName.toStdString()));
            file << j.dump(2);
            file.close();
            m_statusLabel->setText(tr("Exported %1 bookmark(s) to JSON").arg(m_bookmarks.size()));
            util::Logger::Info("[BookmarksPanel] Exported {} bookmarks to {}", m_bookmarks.size(), fileName.toStdString());
        }
        else
        {
            // Export as Markdown
            auto escape = [](const std::string& s) -> QString {
                QString q = QString::fromStdString(s);
                q.replace('|', "\\|");
                q.replace('\n', ' ');
                return q;
            };

            QString md = "# Log Bookmarks\n\n";
            md += "| Row | Category | Label | Timestamp | Summary |\n";
            md += "|-----|----------|-------|-----------|--------|\n";
            for (const auto& bm : m_bookmarks)
            {
                md += QString("| %1 | %2 | %3 | %4 | %5 |\n")
                          .arg(bm.row)
                          .arg(escape(bm.category))
                          .arg(escape(bm.label))
                          .arg(escape(bm.timestamp))
                          .arg(escape(bm.summary));
            }

            std::ofstream file(std::filesystem::path(fileName.toStdString()));
            file << md.toStdString();
            file.close();
            m_statusLabel->setText(tr("Exported %1 bookmark(s) to Markdown").arg(m_bookmarks.size()));
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[BookmarksPanel] Export failed: {}", e.what());
        m_statusLabel->setText(tr("Export failed"));
    }
}

void BookmarksPanel::OnImport()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import Bookmarks"),
        QString(), tr("JSON Files (*.json)"));
    if (fileName.isEmpty()) return;

    try
    {
        std::ifstream file(std::filesystem::path(fileName.toStdString()));
        nlohmann::json j = nlohmann::json::parse(file);

        if (j.contains("categories"))
        {
            m_categories.clear();
            for (const auto& cat : j["categories"])
            {
                m_categories.push_back({
                    cat["name"].get<std::string>(),
                    cat["color"].get<std::string>()
                });
            }
            RebuildCategoryFilter();
        }

        if (j.contains("bookmarks"))
        {
            m_bookmarks.clear();
            for (const auto& item : j["bookmarks"])
            {
                m_bookmarks.push_back({
                    item["row"].get<int>(),
                    item.value("label", std::string{}),
                    item.value("category", std::string{}),
                    item.value("summary", std::string{}),
                    item.value("timestamp", std::string{})
                });
            }
        }

        UpdateCategoryCounts();
        ApplyFilters();
        m_statusLabel->setText(tr("Imported %1 bookmark(s)").arg(m_bookmarks.size()));
        util::Logger::Info("[BookmarksPanel] Imported {} bookmarks", m_bookmarks.size());
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[BookmarksPanel] Import failed: {}", e.what());
        m_statusLabel->setText(tr("Import failed"));
    }
}

void BookmarksPanel::OnSearchTextChanged(const QString&)
{
    ApplyFilters();
}

void BookmarksPanel::OnCategoryFilterChanged(const QString&)
{
    ApplyFilters();
}

void BookmarksPanel::OnAddCategory()
{
    bool ok;
    QString catName = QInputDialog::getText(this, tr("Add Category"),
        tr("Category name:"), QLineEdit::Normal, QString(), &ok);
    if (ok && !catName.isEmpty())
    {
        m_categories.push_back({catName.toStdString(), "#000000"});
        RebuildCategoryFilter();
        m_categoryCombo->addItem(catName);
        m_statusLabel->setText(tr("Added category '%1'").arg(catName));
    }
}

void BookmarksPanel::OnRemoveCategory()
{
    const auto current = m_categoryCombo->currentText().toStdString();
    auto it = std::find_if(m_categories.begin(), m_categories.end(),
        [&current](const Category& c) { return c.name == current; });

    if (it != m_categories.end())
    {
        m_categories.erase(it);
        m_categoryCombo->removeItem(m_categoryCombo->currentIndex());
        RebuildCategoryFilter();
        m_statusLabel->setText(tr("Removed category"));
    }
}

void BookmarksPanel::RebuildTable()
{
    m_table->setRowCount(static_cast<int>(m_filteredBookmarks.size()));
    for (int i = 0; i < static_cast<int>(m_filteredBookmarks.size()); ++i)
    {
        const auto& bm = m_filteredBookmarks[static_cast<size_t>(i)];
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(bm.row)));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(bm.category)));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(bm.label)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(bm.timestamp)));
        m_table->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(bm.summary)));
    }
    m_goToBtn->setEnabled(false);
    m_removeBtn->setEnabled(false);
}

void BookmarksPanel::RebuildCategoryFilter()
{
    int current = m_categoryFilterCombo->currentIndex();
    m_categoryFilterCombo->clear();
    m_categoryFilterCombo->addItem(tr("All"));
    for (const auto& cat : m_categories)
    {
        m_categoryFilterCombo->addItem(QString::fromStdString(cat.name));
    }
    if (current < m_categoryFilterCombo->count())
        m_categoryFilterCombo->setCurrentIndex(current);
}

void BookmarksPanel::UpdateCategoryCounts()
{
    for (auto& cat : m_categories)
    {
        cat.count = static_cast<int>(std::count_if(m_bookmarks.begin(), m_bookmarks.end(),
            [&cat](const Bookmark& b) { return b.category == cat.name; }));
    }
}

void BookmarksPanel::ApplyFilters()
{
    m_filteredBookmarks.clear();
    const QString searchText = m_searchEdit->text().toLower();
    const QString categoryFilter = m_categoryFilterCombo->currentText();

    for (const auto& bm : m_bookmarks)
    {
        // Category filter
        if (categoryFilter != tr("All"))
        {
            if (bm.category != categoryFilter.toStdString())
                continue;
        }

        // Search filter
        if (!searchText.isEmpty())
        {
            QString bookmarkText = (QString::fromStdString(bm.label) + " " +
                                   QString::fromStdString(bm.summary)).toLower();
            if (!bookmarkText.contains(searchText))
                continue;
        }

        m_filteredBookmarks.push_back(bm);
    }

    RebuildTable();
    m_statusLabel->setText(m_filteredBookmarks.empty()
        ? tr("No bookmarks (filtered)")
        : tr("%1 / %2 bookmark(s)").arg(m_filteredBookmarks.size()).arg(m_bookmarks.size()));
}

nlohmann::json BookmarksPanel::GetSessionData() const
{
    nlohmann::json j;
    j["categories"] = nlohmann::json::array();
    for (const auto& cat : m_categories)
    {
        j["categories"].push_back({{"name", cat.name}, {"color", cat.color}});
    }
    j["bookmarks"] = nlohmann::json::array();
    for (const auto& bm : m_bookmarks)
    {
        j["bookmarks"].push_back({
            {"row", bm.row},
            {"label", bm.label},
            {"category", bm.category},
            {"timestamp", bm.timestamp},
            {"summary", bm.summary}
        });
    }
    return j;
}

void BookmarksPanel::LoadSessionData(const nlohmann::json& data)
{
    try
    {
        if (data.contains("categories"))
        {
            m_categories.clear();
            for (const auto& item : data["categories"])
            {
                m_categories.push_back({
                    item.value("name", std::string{}),
                    item.value("color", std::string{})
                });
            }
            RebuildCategoryFilter();
        }

        m_bookmarks.clear();
        if (data.contains("bookmarks"))
        {
            for (const auto& item : data["bookmarks"])
            {
                Bookmark bm;
                bm.row       = item.value("row", -1);
                bm.label     = item.value("label", std::string{});
                bm.category  = item.value("category", std::string{});
                bm.timestamp = item.value("timestamp", std::string{});
                bm.summary   = item.value("summary", std::string{});
                if (bm.row >= 0)
                    m_bookmarks.push_back(std::move(bm));
            }
        }

        UpdateCategoryCounts();
        ApplyFilters();
        util::Logger::Info("[BookmarksPanel] Loaded {} bookmark(s) from session", m_bookmarks.size());
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[BookmarksPanel] Failed to load session data: {}", e.what());
    }
}

int BookmarksPanel::SelectedTableRow() const
{
    const auto sel = m_table->selectedItems();
    if (sel.isEmpty()) return -1;
    return m_table->row(sel.first());
}

} // namespace ui::qt
