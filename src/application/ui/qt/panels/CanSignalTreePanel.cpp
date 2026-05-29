#include "CanSignalTreePanel.hpp"

#include "Logger.hpp"

#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace ui::qt {

CanSignalTreePanel::CanSignalTreePanel(QWidget* parent)
    : QWidget(parent)
{
    BuildLayout();
}

void CanSignalTreePanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_placeholder = new QLabel(
        tr("Load a DBC file to see\nCAN frame and signal structure."), this);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);
    layout->addWidget(m_placeholder);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    m_tree->setToolTip(tr("Check signals to plot.\nCheck a frame to select all its signals."));
    m_tree->hide();
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::itemChanged,
            this, [this](QTreeWidgetItem* item, int col) {
                if (col != 0) return;
                {
                    QSignalBlocker blocker(m_tree);
                    if (item->childCount() > 0)
                    {
                        // Frame toggled: propagate to all child signals.
                        const Qt::CheckState cs =
                            item->checkState(0) != Qt::Unchecked ? Qt::Checked : Qt::Unchecked;
                        for (int i = 0; i < item->childCount(); ++i)
                            item->child(i)->setCheckState(0, cs);
                    }
                    else if (auto* parent = item->parent())
                    {
                        // Signal toggled: update parent aggregate state.
                        int checked = 0;
                        const int total = parent->childCount();
                        for (int i = 0; i < total; ++i)
                            if (parent->child(i)->checkState(0) == Qt::Checked) ++checked;
                        if (checked == 0)
                            parent->setCheckState(0, Qt::Unchecked);
                        else if (checked == total)
                            parent->setCheckState(0, Qt::Checked);
                        else
                            parent->setCheckState(0, Qt::PartiallyChecked);
                    }
                }
                emit SignalSelectionChanged();
            });
}

// ---------------------------------------------------------------------------

void CanSignalTreePanel::SetDatabase(const parser::dbc::DbcDatabase& db)
{
    util::Logger::Debug("[CanSignalTree] Loading DBC database with {} message(s)", db.messages.size());
    m_db = db;
    RebuildTree();
}

std::vector<std::string> CanSignalTreePanel::GetSelectedSignals() const
{
    std::vector<std::string> result;
    QTreeWidgetItemIterator it(m_tree,
        QTreeWidgetItemIterator::Checked | QTreeWidgetItemIterator::NoChildren);
    while (*it)
    {
        const QString key = (*it)->data(0, Qt::UserRole).toString();
        if (!key.isEmpty())
            result.push_back(key.toStdString());
        ++it;
    }
    return result;
}

// ---------------------------------------------------------------------------

void CanSignalTreePanel::RebuildTree()
{
    if (m_db.messages.empty())
    {
        util::Logger::Warn("[CanSignalTree] RebuildTree called with empty DBC database");
        m_tree->hide();
        m_placeholder->show();
        return;
    }

    // Preserve the current checked set across rebuilds.
    const auto prev = GetSelectedSignals();
    const std::set<std::string> checkedSet(prev.begin(), prev.end());
    const bool hadSelection = !prev.empty();

    m_placeholder->hide();

    {
        QSignalBlocker blocker(m_tree);
        m_tree->clear();

        for (const auto& [id, msg] : m_db.messages)
        {
            const QString hexId =
                QString("0x%1").arg(id, 3, 16, QChar('0')).toUpper();
            const QString frameLabel = msg.name.empty()
                ? hexId
                : QString::fromStdString(msg.name) + "  (" + hexId + ')';

            auto* frameItem = new QTreeWidgetItem(m_tree, QStringList{frameLabel});
            frameItem->setFlags(frameItem->flags() | Qt::ItemIsUserCheckable);

            for (const auto& sig : msg.signalDefs)
            {
                const std::string key = "SIG:" + sig.name;
                auto* sigItem = new QTreeWidgetItem(frameItem,
                    QStringList{QString::fromStdString(sig.name)});
                sigItem->setFlags(sigItem->flags() | Qt::ItemIsUserCheckable);
                sigItem->setData(0, Qt::UserRole, QString::fromStdString(key));
                sigItem->setCheckState(0,
                    checkedSet.count(key) ? Qt::Checked : Qt::Unchecked);
            }

            // Set frame check state to reflect its children.
            int checked = 0;
            const int total = frameItem->childCount();
            for (int i = 0; i < total; ++i)
                if (frameItem->child(i)->checkState(0) == Qt::Checked) ++checked;
            if (checked == 0)
                frameItem->setCheckState(0, Qt::Unchecked);
            else if (checked == total)
                frameItem->setCheckState(0, Qt::Checked);
            else
                frameItem->setCheckState(0, Qt::PartiallyChecked);
        }

        m_tree->expandAll();
    }

    m_tree->show();

    // Count total signals across all messages for the log.
    size_t totalSignals = 0;
    for (const auto& [id, msg] : m_db.messages)
        totalSignals += msg.signalDefs.size();
    util::Logger::Info("[CanSignalTree] Signal tree populated: {} frame(s), {} signal(s)",
        m_db.messages.size(), totalSignals);

    if (hadSelection || !GetSelectedSignals().empty())
        emit SignalSelectionChanged();
}

} // namespace ui::qt
