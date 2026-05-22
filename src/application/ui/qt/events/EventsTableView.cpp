#include "EventsTableView.hpp"

#include "Logger.hpp"
#include "EventsContainer.hpp"
#include "EventsTableModel.hpp"

#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QAction>
#include <QClipboard>
#include <QGuiApplication>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>
#include <limits>
#include <unordered_map>

namespace ui::qt
{

EventsTableView::EventsTableView(
    db::EventsContainer& events, QWidget* parent)
    : QTableView(parent)
    , m_events(events)
{
    m_model = new EventsTableModel(m_events, this);
    setModel(m_model);

    InitializeView();
    ConnectSelectionSignals();
}

void EventsTableView::InitializeView()
{
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionsMovable(true);
    horizontalHeader()->setSectionsClickable(true);
    verticalHeader()->setVisible(false);
    setSortingEnabled(true);
    ResizeColumnsToConfiguration();

    auto* copyAction = new QAction(tr("Copy"), this);
    copyAction->setShortcut(QKeySequence::Copy);
    addAction(copyAction);

    connect(copyAction, &QAction::triggered, this, [this]() {
        QModelIndexList indexes = selectionModel()
                                      ? selectionModel()->selectedIndexes()
                                      : QModelIndexList {};
        if (indexes.isEmpty())
            return;

        // Sort by row/column
        std::sort(indexes.begin(), indexes.end(),
            [](const QModelIndex& a, const QModelIndex& b) {
                if (a.row() == b.row())
                    return a.column() < b.column();
                return a.row() < b.row();
            });

        QStringList lines;
        int currentRow = indexes.first().row();
        QStringList rowValues;

        for (const auto& idx : indexes)
        {
            if (idx.row() != currentRow)
            {
                lines << rowValues.join('\t');
                rowValues.clear();
                currentRow = idx.row();
            }
            rowValues << model()->data(idx, Qt::DisplayRole).toString();
        }
        if (!rowValues.isEmpty())
            lines << rowValues.join('\t');

        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(lines.join('\n'));
    });

    m_events.RegisterOndDataUpdated(this);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QTableView::customContextMenuRequested,
            this, &EventsTableView::ShowContextMenu);
}

void EventsTableView::ConnectSelectionSignals()
{
    connect(selectionModel(), &QItemSelectionModel::currentRowChanged, this,
        [this](const QModelIndex& current, const QModelIndex&)
        {
            if (!current.isValid())
            {
                util::Logger::Debug("[EventsTableView] currentRowChanged: invalid -> -1");
                emit CurrentActualRowChanged(-1);
                return;
            }

            const int actualIndex = m_model->ResolveToActualIndex(current.row());
            util::Logger::Debug(
                "[EventsTableView] currentRowChanged viewRow={} actualIndex={}",
                current.row(), actualIndex);

            emit CurrentActualRowChanged(actualIndex);
            if (actualIndex >= 0)
                m_events.SetCurrentItem(actualIndex);

           
        });
}

void EventsTableView::ResizeColumnsToConfiguration()
{
    const auto widths = m_model->ColumnWidths();
    for (int column = 0; column < static_cast<int>(widths.size()); ++column)
    {
        const auto width = widths[static_cast<std::size_t>(column)];
        horizontalHeader()->resizeSection(column, width);
    }
}

void EventsTableView::RefreshColumns()
{
    if (!m_model)
        return;

    m_model->RefreshColumns();
    ResizeColumnsToConfiguration();
    
    // Force header to recalculate stretch section
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setStretchLastSection(true);
}

void EventsTableView::RefreshView()
{
    if (!m_model)
        return;

    m_model->SyncWithContainer();
    viewport()->update();
}

void EventsTableView::SetFilteredEvents(
    const std::vector<unsigned long>& filteredIndices)
{
    if (!m_model)
        return;

    m_model->SetFilteredIndices(filteredIndices);
    viewport()->update();
}

void EventsTableView::ClearFilter()
{
    if (!m_model)
        return;

    m_model->ClearFilter();
    viewport()->update();
}

const std::vector<unsigned long>* EventsTableView::GetFilteredIndices() const
{
    if (!m_model)
        return nullptr;
    
    const auto& indices = m_model->GetFilteredIndices();
    return indices.empty() ? nullptr : &indices;
}

void EventsTableView::UpdateColors()
{
    if (!m_model)
        return;

    m_model->UpdateColors();
    viewport()->update();
}

void EventsTableView::OnDataUpdated()
{
    RefreshView();
}

void EventsTableView::OnCurrentIndexUpdated(const int index)
{
    util::Logger::Debug("[EventsTableView] OnCurrentIndexUpdated index={}", index);

    if (index < 0)
    {
        if (selectionModel())
            selectionModel()->clearSelection();
        return;
    }    

    // Ensure the view scrolls and selects the corresponding row
    ScrollToActualRow(index);
}

int EventsTableView::CurrentActualRow() const
{
    if (!selectionModel())
        return -1;

    const QModelIndex current = selectionModel()->currentIndex();
    if (!current.isValid())
        return -1;

    return m_model->ResolveToActualIndex(current.row());
}

void EventsTableView::ScrollToActualRow(int actualRow)
{
    util::Logger::Debug("[EventsTableView] ScrollToActualRow actualRow={}", actualRow);

    if (actualRow < 0 || !m_model)
        return;

    const int viewRow = m_model->RowFromActualIndex(actualRow);
    util::Logger::Debug("[EventsTableView] mapped actualRow={} to viewRow={}",
        actualRow, viewRow);
    if (viewRow < 0)
        return;

    const QModelIndex idx = m_model->index(viewRow, 0);
    if (!idx.isValid())
    {
        util::Logger::Warn("[EventsTableView] index invalid for viewRow={}", viewRow);
        return;
    }

    // Only scroll if the row is outside the visible area
    const QRect visibleRect = viewport()->rect();
    const QModelIndex topIndex = indexAt(visibleRect.topLeft());
    const QModelIndex bottomIndex = indexAt(visibleRect.bottomLeft());
    const int topRow = topIndex.isValid() ? topIndex.row() : -1;
    const int bottomRow = bottomIndex.isValid() ? bottomIndex.row() : -1;

    if (topRow == -1 || bottomRow == -1 || viewRow < topRow || viewRow > bottomRow)
    {
        if (!hasFocus())
            setFocus(Qt::OtherFocusReason);

        scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }

    if (selectionModel())
    {
        selectionModel()->setCurrentIndex(
            idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        util::Logger::Debug("[EventsTableView] selection set to viewRow={}", viewRow);
    }

}

// ---------------------------------------------------------------------------
// Search / highlight
// ---------------------------------------------------------------------------

void EventsTableView::SetSearchTerm(const QString& term, bool caseSensitive)
{
    m_model->SetSearchTerm(term, caseSensitive);
    m_currentMatchIndex = -1;

    const int total = m_model->MatchCount();
    if (total > 0)
    {
        m_currentMatchIndex = 0;
        ScrollToMatchIndex(0);
    }
    emit MatchInfoChanged(total > 0 ? 1 : 0, total);
}

void EventsTableView::NavigateToNextMatch()
{
    const int total = m_model->MatchCount();
    if (total == 0) return;

    m_currentMatchIndex = (m_currentMatchIndex + 1) % total;
    ScrollToMatchIndex(m_currentMatchIndex);
    emit MatchInfoChanged(m_currentMatchIndex + 1, total);
}

void EventsTableView::NavigateToPrevMatch()
{
    const int total = m_model->MatchCount();
    if (total == 0) return;

    m_currentMatchIndex = (m_currentMatchIndex - 1 + total) % total;
    ScrollToMatchIndex(m_currentMatchIndex);
    emit MatchInfoChanged(m_currentMatchIndex + 1, total);
}

void EventsTableView::ScrollToMatchIndex(int matchIndex)
{
    const auto& rows = m_model->MatchedRows();
    if (matchIndex < 0 || matchIndex >= static_cast<int>(rows.size())) return;

    const QModelIndex idx = m_model->index(rows[static_cast<size_t>(matchIndex)], 0);
    if (!idx.isValid()) return;

    scrollTo(idx, QAbstractItemView::PositionAtCenter);
    if (selectionModel())
        selectionModel()->setCurrentIndex(
            idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void EventsTableView::ShowContextMenu(const QPoint& pos)
{
    const int row = CurrentActualRow();
    const bool hasSelection = !SelectedActualIndices().empty();

    QMenu menu(this);

    auto* copyAction = menu.addAction(tr("Copy\tCtrl+C"));

    auto* copyAsMenu = menu.addMenu(tr("Copy as…"));
    auto* copyJsonAction = copyAsMenu->addAction(tr("JSON"));
    copyJsonAction->setEnabled(hasSelection);
    auto* copyCsvAction  = copyAsMenu->addAction(tr("CSV (with header)"));
    copyCsvAction->setEnabled(hasSelection);

    menu.addSeparator();
    auto* bookmarkAction = menu.addAction(tr("Bookmark Event…"));
    bookmarkAction->setEnabled(row >= 0);
    auto* scenarioAction = menu.addAction(tr("Add to Scenario…"));
    scenarioAction->setEnabled(row >= 0);

    QAction* chosen = menu.exec(viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == copyAction)
    {
        for (QAction* a : actions())
        {
            if (a->shortcut() == QKeySequence::Copy)
            {
                a->trigger();
                break;
            }
        }
    }
    else if (chosen == copyJsonAction)  { CopySelectedRowsAsJson(); }
    else if (chosen == copyCsvAction)   { CopySelectedRowsAsCsv();  }
    else if (chosen == bookmarkAction && row >= 0)
    {
        emit BookmarkRequested(row);
    }
    else if (chosen == scenarioAction && row >= 0)
    {
        emit AddToScenarioRequested(row);
    }
}

// ---------------------------------------------------------------------------
// Copy as JSON / CSV
// ---------------------------------------------------------------------------

std::vector<int> EventsTableView::SelectedActualIndices() const
{
    if (!selectionModel() || !m_model) return {};
    const QModelIndexList selected = selectionModel()->selectedRows();
    std::vector<int> result;
    result.reserve(static_cast<size_t>(selected.size()));
    for (const QModelIndex& idx : selected)
    {
        const int actual = m_model->ResolveToActualIndex(idx.row());
        if (actual >= 0)
            result.push_back(actual);
    }
    std::sort(result.begin(), result.end());
    return result;
}

static QString JsonEscape(const QString& s)
{
    QString r;
    r.reserve(s.size() + 4);
    for (const QChar c : s) {
        switch (c.unicode()) {
            case '"':  r += QLatin1String("\\\""); break;
            case '\\': r += QLatin1String("\\\\"); break;
            case '\n': r += QLatin1String("\\n");  break;
            case '\r': r += QLatin1String("\\r");  break;
            case '\t': r += QLatin1String("\\t");  break;
            default:   r += c;                      break;
        }
    }
    return r;
}

static QString CsvEscape(const QString& s)
{
    if (s.contains(u',') || s.contains(u'"') || s.contains(u'\n'))
        return u'"' + QString(s).replace(u'"', QLatin1String("\"\"")) + u'"';
    return s;
}

void EventsTableView::CopySelectedRowsAsJson()
{
    const auto indices = SelectedActualIndices();
    if (indices.empty()) return;

    const bool multi = indices.size() > 1;
    QString out;
    out.reserve(512 * static_cast<int>(indices.size()));
    if (multi) out += QLatin1String("[\n");

    for (size_t i = 0; i < indices.size(); ++i) {
        const auto& event = m_events.GetEvent(indices[i]);
        if (multi) out += QLatin1String("  ");
        out += QLatin1String("{\n");

        const QString indent = multi ? QLatin1String("    ") : QLatin1String("  ");
        out += indent + QLatin1String("\"id\": ") + QString::number(event.getId());

        for (const auto& [key, value] : event.getEventItems()) {
            out += QLatin1String(",\n") + indent
                + u'"' + JsonEscape(QString::fromStdString(key)) + QLatin1String("\": \"")
                + JsonEscape(QString::fromStdString(value)) + u'"';
        }
        out += u'\n';
        if (multi) out += QLatin1String("  }");
        else        out += u'}';

        if (multi && i + 1 < indices.size()) out += u',';
        if (multi) out += u'\n';
    }

    if (multi) out += u']';
    QGuiApplication::clipboard()->setText(out);
}

void EventsTableView::CopySelectedRowsAsCsv()
{
    const auto indices = SelectedActualIndices();
    if (indices.empty()) return;

    // Collect field names in insertion order across all selected events.
    std::vector<std::string> fieldOrder;
    fieldOrder.push_back("id");
    std::unordered_map<std::string, int> seen;
    seen["id"] = 0;
    for (const int idx : indices) {
        for (const auto& [key, value] : m_events.GetEvent(idx).getEventItems()) {
            if (seen.emplace(key, static_cast<int>(fieldOrder.size())).second)
                fieldOrder.push_back(key);
        }
    }

    QString out;
    out.reserve(256 * static_cast<int>(indices.size() + 1));

    // Header row
    for (size_t col = 0; col < fieldOrder.size(); ++col) {
        if (col) out += u',';
        out += CsvEscape(QString::fromStdString(fieldOrder[col]));
    }
    out += u'\n';

    // Data rows
    for (const int idx : indices) {
        const auto& event = m_events.GetEvent(idx);
        for (size_t col = 0; col < fieldOrder.size(); ++col) {
            if (col) out += u',';
            const std::string val = (fieldOrder[col] == "id")
                ? std::to_string(event.getId())
                : event.findByKey(fieldOrder[col]);
            out += CsvEscape(QString::fromStdString(val));
        }
        out += u'\n';
    }

    QGuiApplication::clipboard()->setText(out);
}

// ---------------------------------------------------------------------------
// Jump to timestamp
// ---------------------------------------------------------------------------

void EventsTableView::JumpToTimestamp()
{
    bool ok = false;
    const QString input = QInputDialog::getText(
        this, tr("Go to Timestamp"),
        tr("Timestamp (seconds, e.g. 1234.567):"),
        QLineEdit::Normal, {}, &ok);
    if (!ok || input.trimmed().isEmpty()) return;

    const double target = input.trimmed().toDouble(&ok);
    if (!ok) {
        QMessageBox::warning(this, tr("Go to Timestamp"),
            tr("Could not parse \"%1\" as a numeric timestamp.").arg(input));
        return;
    }

    const int rows = m_model ? m_model->rowCount({}) : 0;
    if (rows == 0) return;

    int bestRow = -1;
    double bestDiff = std::numeric_limits<double>::max();

    for (int row = 0; row < rows; ++row) {
        const int actual = m_model->ResolveToActualIndex(row);
        if (actual < 0) continue;
        const std::string ts = m_events.GetEvent(actual).findByKey("timestamp");
        if (ts.empty()) continue;
        bool tsOk = false;
        const double t = QString::fromStdString(ts).toDouble(&tsOk);
        if (!tsOk) continue;
        const double diff = std::fabs(t - target);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestRow  = row;
        }
    }

    if (bestRow < 0) {
        QMessageBox::information(this, tr("Go to Timestamp"),
            tr("No events with a numeric timestamp field were found."));
        return;
    }

    const int actual = m_model->ResolveToActualIndex(bestRow);
    ScrollToActualRow(actual);
}

} // namespace ui::qt
