#include "PluginManagerDialog.hpp"

#include "PluginManager.hpp"
#include "IPlugin.hpp"
#include "Logger.hpp"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace ui::qt
{

namespace {

static const int kColName    = 0;
static const int kColId      = 1;
static const int kColVersion = 2;
static const int kColType    = 3;
static const int kColStatus  = 4;
static const int kColAutoLoad= 5;
static const int kColPath    = 6;
static const int kNumCols    = 7;

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PluginManagerDialog::PluginManagerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage Plugins"));
    setMinimumSize(820, 480);
    BuildUi();
    PopulateTable();
}

void PluginManagerDialog::BuildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // ── Heading ───────────────────────────────────────────────────────────
    auto* heading = new QLabel(tr(
        "Plugins directory: <b>%1</b>")
            .arg(QString::fromStdString(
                plugin::PluginManager::GetInstance()
                    .GetPluginsDirectory().string())));
    heading->setWordWrap(true);
    root->addWidget(heading);

    // ── Table ─────────────────────────────────────────────────────────────
    m_table = new QTableWidget(0, kNumCols, this);
    m_table->setHorizontalHeaderLabels({
        tr("Name"), tr("ID"), tr("Version"),
        tr("Type"), tr("Status"), tr("Auto-load"), tr("Path")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(kColName,    QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(kColId,      QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(kColVersion, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColType,    QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColStatus,  QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColAutoLoad,QHeaderView::ResizeToContents);
    m_table->setColumnWidth(kColName, 160);
    m_table->setColumnWidth(kColId,   180);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    root->addWidget(m_table, 1);

    // ── Action buttons ────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();

    m_installBtn = new QPushButton(tr("Install…"), this);
    m_installBtn->setToolTip(
        tr("Install a plugin from a .zip or shared library file.\n"
           "The file is copied to the plugins directory and loaded."));
    btnRow->addWidget(m_installBtn);

    m_uninstallBtn = new QPushButton(tr("Uninstall"), this);
    m_uninstallBtn->setToolTip(tr("Remove the selected plugin file from the plugins directory."));
    m_uninstallBtn->setEnabled(false);
    btnRow->addWidget(m_uninstallBtn);

    m_enableBtn = new QPushButton(tr("Enable"), this);
    m_enableBtn->setToolTip(tr("Load the selected plugin into this session."));
    m_enableBtn->setEnabled(false);
    btnRow->addWidget(m_enableBtn);

    m_disableBtn = new QPushButton(tr("Disable"), this);
    m_disableBtn->setToolTip(tr("Unload the selected plugin from this session (keeps the file)."));
    m_disableBtn->setEnabled(false);
    btnRow->addWidget(m_disableBtn);

    btnRow->addStretch();

    auto* refreshBtn = new QPushButton(tr("Refresh"), this);
    refreshBtn->setToolTip(tr("Rescan the plugins directory."));
    btnRow->addWidget(refreshBtn);

    root->addLayout(btnRow);

    // ── Status label ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    root->addWidget(m_statusLabel);

    // ── Close button ──────────────────────────────────────────────────────
    auto* closeBB = new QDialogButtonBox(QDialogButtonBox::Close, this);
    root->addWidget(closeBB);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_installBtn,   &QPushButton::clicked, this, &PluginManagerDialog::OnInstall);
    connect(m_uninstallBtn, &QPushButton::clicked, this, &PluginManagerDialog::OnUninstall);
    connect(m_enableBtn,    &QPushButton::clicked, this, &PluginManagerDialog::OnEnable);
    connect(m_disableBtn,   &QPushButton::clicked, this, &PluginManagerDialog::OnDisable);
    connect(refreshBtn,     &QPushButton::clicked, this, &PluginManagerDialog::OnRefresh);
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &PluginManagerDialog::OnSelectionChanged);
    connect(closeBB, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ---------------------------------------------------------------------------
// Table population
// ---------------------------------------------------------------------------

void PluginManagerDialog::PopulateTable()
{
    m_table->setSortingEnabled(false);
    m_table->clearContents();

    auto& mgr = plugin::PluginManager::GetInstance();

    // Collect discovered paths and loaded plugin info.
    const auto discovered     = mgr.DiscoverPlugins();
    const auto& loadedPlugins = mgr.GetLoadedPlugins();

    // Build a set of paths already loaded so we can mark them.
    std::map<std::filesystem::path, const plugin::PluginLoadInfo*> loadedByPath;
    for (const auto& [id, info] : loadedPlugins)
        loadedByPath[info.path] = &info;

    // Merge: loaded plugins first, then any discovered-but-not-loaded paths.
    std::vector<std::pair<std::filesystem::path, const plugin::PluginLoadInfo*>> rows;
    for (const auto& [id, info] : loadedPlugins)
        rows.emplace_back(info.path, &info);
    for (const auto& p : discovered)
    {
        if (loadedByPath.find(p) == loadedByPath.end())
            rows.emplace_back(p, nullptr);
    }

    m_table->setRowCount(static_cast<int>(rows.size()));

    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        const auto& [path, info] = rows[static_cast<size_t>(i)];
        const bool loaded = (info != nullptr);

        plugin::PluginMetadata meta;
        if (loaded && info->instance)
            meta = info->instance->GetMetadata();
        else
        {
            // Fill minimal info from the file path for not-yet-loaded plugins.
            meta.name = path.stem().string();
            meta.id   = meta.name;
        }

        auto* nameItem = new QTableWidgetItem(QString::fromStdString(meta.name));
        nameItem->setData(Qt::UserRole, QString::fromStdString(meta.id));
        m_table->setItem(i, kColName,    nameItem);
        m_table->setItem(i, kColId,      new QTableWidgetItem(QString::fromStdString(meta.id)));
        m_table->setItem(i, kColVersion, new QTableWidgetItem(QString::fromStdString(meta.version)));
        m_table->setItem(i, kColType,    new QTableWidgetItem(TypeName(static_cast<int>(meta.type))));

        const QString status = loaded ? tr("Loaded") : tr("Not loaded");
        auto* statusItem = new QTableWidgetItem(status);
        statusItem->setForeground(loaded ? QColor(Qt::darkGreen) : QColor(Qt::gray));
        m_table->setItem(i, kColStatus, statusItem);

        const QString autoLoadStr = (loaded && info->autoLoad) ? tr("Yes") : tr("No");
        m_table->setItem(i, kColAutoLoad, new QTableWidgetItem(autoLoadStr));
        m_table->setItem(i, kColPath,
            new QTableWidgetItem(QString::fromStdString(path.string())));
    }

    m_table->setSortingEnabled(true);
    m_statusLabel->setText(
        tr("%1 plugin(s) discovered, %2 loaded")
            .arg(rows.size())
            .arg(loadedPlugins.size()));

    OnSelectionChanged();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void PluginManagerDialog::OnInstall()
{
    const QString docs =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Install Plugin"), docs,
        tr("Plugin files (*.zip *.dylib *.so *.dll);; All files (*.*)"));
    if (path.isEmpty()) return;

    auto& mgr = plugin::PluginManager::GetInstance();
    auto regResult = mgr.RegisterPlugin(
        std::filesystem::path(path.toStdString()));
    if (!regResult.isOk())
    {
        QMessageBox::critical(this, tr("Install Plugin"),
            tr("Failed to install plugin:\n%1")
                .arg(QString::fromStdString(regResult.error().what())));
        return;
    }

    const std::string pluginId = regResult.unwrap();
    util::Logger::Info("[PluginManager] Installed and loaded plugin '{}'", pluginId);
    m_statusLabel->setText(tr("Installed: %1").arg(QString::fromStdString(pluginId)));
    PopulateTable();
}

void PluginManagerDialog::OnUninstall()
{
    const int row = m_table->currentRow();
    if (row < 0) return;

    const QString id = m_table->item(row, kColName)
                           ->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    const auto answer = QMessageBox::question(
        this, tr("Uninstall Plugin"),
        tr("Uninstall plugin \"%1\"?\nThe plugin file will be removed from the plugins directory.")
            .arg(id),
        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    auto& mgr = plugin::PluginManager::GetInstance();
    const auto result = mgr.UnregisterPlugin(id.toStdString());
    if (!result.isOk())
    {
        QMessageBox::critical(this, tr("Uninstall Plugin"),
            tr("Failed to uninstall:\n%1")
                .arg(QString::fromStdString(result.error().what())));
        return;
    }

    util::Logger::Info("[PluginManager] Uninstalled plugin '{}'", id.toStdString());
    m_statusLabel->setText(tr("Uninstalled: %1").arg(id));
    PopulateTable();
}

void PluginManagerDialog::OnEnable()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    const QString id = m_table->item(row, kColName)
                           ->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    auto& mgr = plugin::PluginManager::GetInstance();
    const auto result = mgr.EnablePlugin(id.toStdString());
    if (!result.isOk())
    {
        QMessageBox::critical(this, tr("Enable Plugin"),
            tr("Failed to enable plugin \"%1\":\n%2")
                .arg(id)
                .arg(QString::fromStdString(result.error().what())));
        return;
    }

    util::Logger::Info("[PluginManager] Enabled plugin '{}'", id.toStdString());
    m_statusLabel->setText(tr("Enabled: %1").arg(id));
    PopulateTable();
}

void PluginManagerDialog::OnDisable()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    const QString id = m_table->item(row, kColName)
                           ->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    auto& mgr = plugin::PluginManager::GetInstance();
    const auto result = mgr.DisablePlugin(id.toStdString());
    if (!result.isOk())
    {
        QMessageBox::critical(this, tr("Disable Plugin"),
            tr("Failed to disable plugin \"%1\":\n%2")
                .arg(id)
                .arg(QString::fromStdString(result.error().what())));
        return;
    }

    util::Logger::Info("[PluginManager] Disabled plugin '{}'", id.toStdString());
    m_statusLabel->setText(tr("Disabled: %1").arg(id));
    PopulateTable();
}

void PluginManagerDialog::OnRefresh()
{
    PopulateTable();
}

void PluginManagerDialog::OnSelectionChanged()
{
    const int row = m_table->currentRow();
    const bool sel = (row >= 0);

    bool isLoaded = false;
    if (sel && m_table->item(row, kColStatus))
        isLoaded = (m_table->item(row, kColStatus)->text() == tr("Loaded"));

    m_uninstallBtn->setEnabled(sel);
    m_enableBtn->setEnabled(sel && !isLoaded);
    m_disableBtn->setEnabled(sel && isLoaded);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString PluginManagerDialog::TypeName(int type) const
{
    using PT = plugin::PluginType;
    switch (static_cast<PT>(type))
    {
        case PT::Parser:        return tr("Parser");
        case PT::Filter:        return tr("Filter");
        case PT::FieldConversion: return tr("Field Conversion");
        case PT::Exporter:      return tr("Exporter");
        case PT::Analyzer:      return tr("Analyzer");
        case PT::AIProvider:    return tr("AI Provider");
        case PT::Connector:     return tr("Connector");
        case PT::Visualizer:    return tr("Visualizer");
        case PT::Custom:        return tr("Custom");
        default:                return tr("Unknown");
    }
}

} // namespace ui::qt
