#include "UpdateDialog.hpp"

#include "Version.hpp"

#include <QCheckBox>
#include <QDesktopServices>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace ui::qt {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

UpdateDialog::UpdateDialog(const updates::UpdateCheckResult& result,
                           UpdateChecker*                   checker,
                           QWidget*                         parent)
    : QDialog(parent), checker(checker), m_result(result)
{
    setWindowTitle(tr("Check for Updates"));
    setMinimumWidth(640);
    BuildLayout(result);

    connect(checker, &UpdateChecker::PluginDownloadProgress,
            this,    &UpdateDialog::OnPluginProgress);
    connect(checker, &UpdateChecker::PluginDownloadComplete,
            this,    &UpdateDialog::OnPluginComplete);
    connect(checker, &UpdateChecker::PluginDownloadFailed,
            this,    &UpdateDialog::OnPluginFailed);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void UpdateDialog::BuildLayout(const updates::UpdateCheckResult& result)
{
    auto* outer = new QVBoxLayout(this);
    outer->setSpacing(12);
    outer->setContentsMargins(16, 16, 16, 16);

    // ── App update section ───────────────────────────────────────────────
    const auto& cur = Version::current();
    const QString curStr = QString::fromStdString(cur.asShortStr()).trimmed();

    auto* appBox    = new QGroupBox(tr("Application"), this);
    auto* appLayout = new QVBoxLayout(appBox);

    if (result.app.isNewer)
    {
        auto* verLabel = new QLabel(
            tr("<b>New version available:</b> %1 &nbsp;&nbsp; <i>(current: %2)</i>")
                .arg(QString::fromStdString(result.app.version), curStr),
            appBox);
        appLayout->addWidget(verLabel);

        auto* notes = new QTextBrowser(appBox);
        notes->setMarkdown(QString::fromStdString(result.app.releaseNotes));
        notes->setMaximumHeight(160);
        notes->setReadOnly(true);
        appLayout->addWidget(notes);

        auto* openBtn = new QPushButton(tr("Open Release Page"), appBox);
        const QString url = QString::fromStdString(result.app.releaseUrl);
        connect(openBtn, &QPushButton::clicked, this, [url]() {
            QDesktopServices::openUrl(QUrl(url));
        });
        auto* btnRow = new QHBoxLayout();
        btnRow->addStretch();
        btnRow->addWidget(openBtn);
        appLayout->addLayout(btnRow);
    }
    else
    {
        appLayout->addWidget(new QLabel(
            tr("You are running the latest version (%1).").arg(curStr), appBox));
    }
    outer->addWidget(appBox);

    // ── Plugin updates section ────────────────────────────────────────────
    auto* pluginBox    = new QGroupBox(tr("Plugin Updates"), this);
    auto* pluginLayout = new QVBoxLayout(pluginBox);

    if (result.plugins.empty())
    {
        pluginLayout->addWidget(new QLabel(tr("All plugins are up to date."), pluginBox));
    }
    else
    {
        m_pluginTable = new QTableWidget(
            static_cast<int>(result.plugins.size()), 5, pluginBox);
        m_pluginTable->setHorizontalHeaderLabels(
            {tr("Plugin"), tr("Installed"), tr("Available"), tr("API"), tr("Update")});
        m_pluginTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_pluginTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_pluginTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_pluginTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_pluginTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        m_pluginTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_pluginTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_pluginTable->verticalHeader()->hide();

        for (int r = 0; r < static_cast<int>(result.plugins.size()); ++r)
        {
            const auto& p = result.plugins[static_cast<size_t>(r)];

            m_pluginTable->setItem(r, 0, new QTableWidgetItem(
                QString::fromStdString(p.name.empty() ? p.id : p.name)));
            m_pluginTable->setItem(r, 1, new QTableWidgetItem(
                p.currentVersion.empty()
                    ? tr("—")
                    : QString::fromStdString(p.currentVersion)));
            m_pluginTable->setItem(r, 2, new QTableWidgetItem(
                QString::fromStdString(p.availableVersion)));

            auto* apiItem = new QTableWidgetItem(
                p.isCompatible ? tr("✓") : tr("⚠ Incompatible"));
            apiItem->setForeground(p.isCompatible
                                       ? QColor(Qt::green)
                                       : QColor(Qt::red));
            m_pluginTable->setItem(r, 3, apiItem);

            // Checkbox in the last column; disabled if incompatible
            auto* cb = new QCheckBox(pluginBox);
            cb->setEnabled(p.isCompatible);
            if (p.isCompatible)
                cb->setChecked(true); // default: select all compatible
            // Center the checkbox in the cell
            auto* wrapper  = new QWidget(pluginBox);
            auto* wlayout  = new QHBoxLayout(wrapper);
            wlayout->setContentsMargins(0, 0, 0, 0);
            wlayout->setAlignment(Qt::AlignCenter);
            wlayout->addWidget(cb);
            wrapper->setLayout(wlayout);
            // Store pluginId in the checkbox's object name for retrieval
            cb->setObjectName(QString::fromStdString(p.id));
            m_pluginTable->setCellWidget(r, 4, wrapper);
        }

        pluginLayout->addWidget(m_pluginTable);

        m_updateBtn = new QPushButton(tr("Update Selected"), pluginBox);
        connect(m_updateBtn, &QPushButton::clicked,
                this,         &UpdateDialog::OnUpdateSelectedClicked);
        auto* updateRow = new QHBoxLayout();
        updateRow->addStretch();
        updateRow->addWidget(m_updateBtn);
        pluginLayout->addLayout(updateRow);
    }
    outer->addWidget(pluginBox);

    // ── Close button ──────────────────────────────────────────────────────
    auto* closeBtn = new QPushButton(tr("Close"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto* closeRow = new QHBoxLayout();
    closeRow->addStretch();
    closeRow->addWidget(closeBtn);
    outer->addLayout(closeRow);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void UpdateDialog::OnUpdateSelectedClicked()
{
    if (!m_pluginTable || !checker) return;

    m_pendingDownloads = 0;

    for (int r = 0; r < m_pluginTable->rowCount(); ++r)
    {
        auto* wrapper = m_pluginTable->cellWidget(r, 4);
        if (!wrapper) continue;
        auto* cb = wrapper->findChild<QCheckBox*>();
        if (!cb || !cb->isChecked() || !cb->isEnabled()) continue;

        const QString pluginId = cb->objectName();

        // Find matching info
        for (const auto& p : m_result.plugins)
        {
            if (QString::fromStdString(p.id) == pluginId)
            {
                ++m_pendingDownloads;
                checker->DownloadPlugin(p);
                // Replace checkbox cell with progress bar
                auto* prog = new QProgressBar(m_pluginTable);
                prog->setRange(0, 100);
                prog->setValue(0);
                prog->setObjectName(pluginId);
                m_pluginTable->setCellWidget(r, 4, prog);
                break;
            }
        }
    }

    if (m_pendingDownloads == 0)
    {
        QMessageBox::information(this, tr("Update"),
                                 tr("No plugins selected for update."));
        return;
    }

    m_updateBtn->setEnabled(false);
}

void UpdateDialog::OnPluginProgress(QString pluginId, qint64 received, qint64 total)
{
    if (!m_pluginTable) return;
    for (int r = 0; r < m_pluginTable->rowCount(); ++r)
    {
        auto* w = m_pluginTable->cellWidget(r, 4);
        if (!w) continue;
        auto* prog = qobject_cast<QProgressBar*>(w);
        if (prog && prog->objectName() == pluginId && total > 0)
        {
            prog->setValue(static_cast<int>(received * 100 / total));
            break;
        }
    }
}

void UpdateDialog::OnPluginComplete(QString pluginId, QString tempPath)
{
    SetRowProgress(m_pluginTable ? m_pluginTable->rowCount() : 0, 100);

    // Mark row with checkmark
    if (m_pluginTable)
    {
        for (int r = 0; r < m_pluginTable->rowCount(); ++r)
        {
            auto* w = m_pluginTable->cellWidget(r, 4);
            if (!w) continue;
            auto* prog = qobject_cast<QProgressBar*>(w);
            if (prog && prog->objectName() == pluginId)
            {
                auto* done = new QLabel(tr("✅ Done"), m_pluginTable);
                done->setAlignment(Qt::AlignCenter);
                m_pluginTable->setCellWidget(r, 4, done);
                break;
            }
        }
    }

    emit ApplyPluginUpdate(pluginId, tempPath);

    --m_pendingDownloads;
    if (m_pendingDownloads == 0 && m_updateBtn)
        m_updateBtn->setEnabled(true);
}

void UpdateDialog::OnPluginFailed(QString pluginId, QString error)
{
    if (m_pluginTable)
    {
        for (int r = 0; r < m_pluginTable->rowCount(); ++r)
        {
            auto* w = m_pluginTable->cellWidget(r, 4);
            if (!w) continue;
            auto* prog = qobject_cast<QProgressBar*>(w);
            if (prog && prog->objectName() == pluginId)
            {
                auto* fail = new QLabel(tr("❌"), m_pluginTable);
                fail->setAlignment(Qt::AlignCenter);
                fail->setToolTip(error);
                m_pluginTable->setCellWidget(r, 4, fail);
                break;
            }
        }
    }

    QMessageBox::warning(this, tr("Update Failed"),
                         tr("Failed to update plugin %1:\n%2").arg(pluginId, error));

    --m_pendingDownloads;
    if (m_pendingDownloads == 0 && m_updateBtn)
        m_updateBtn->setEnabled(true);
}

void UpdateDialog::SetRowProgress(int /*row*/, int /*percent*/)
{
    // Individual row progress is handled in OnPluginProgress via objectName lookup.
}

} // namespace ui::qt
