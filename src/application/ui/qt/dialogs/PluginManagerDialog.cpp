#include "PluginManagerDialog.hpp"

#include "IPlugin.hpp"
#include "Logger.hpp"
#include "PluginManager.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
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
static const int kNumCols    = 6;

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PluginManagerDialog::PluginManagerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage Plugins"));
    setMinimumSize(860, 560);
    BuildUi();
    PopulateTable();
}

void PluginManagerDialog::BuildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // ── Heading ───────────────────────────────────────────────────────────
    auto* heading = new QLabel(
        tr("Plugins directory: <b>%1</b>")
            .arg(QString::fromStdString(
                plugin::PluginManager::GetInstance()
                    .GetPluginsDirectory().string())));
    heading->setWordWrap(true);
    root->addWidget(heading);

    // ── Splitter: table (top) + details (bottom) ──────────────────────────
    auto* splitter = new QSplitter(Qt::Vertical, this);
    root->addWidget(splitter, 1);

    // ── Plugin table ──────────────────────────────────────────────────────
    auto* tableBox    = new QGroupBox(tr("Available Plugins"), splitter);
    auto* tableLayout = new QVBoxLayout(tableBox);

    m_table = new QTableWidget(0, kNumCols, tableBox);
    m_table->setHorizontalHeaderLabels({
        tr("Name"), tr("ID"), tr("Version"),
        tr("Type"), tr("Status"), tr("Auto-load")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(kColName,    QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(kColId,      QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(kColVersion, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColType,    QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColStatus,  QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kColAutoLoad,QHeaderView::ResizeToContents);
    m_table->setColumnWidth(kColName, 160);
    m_table->setColumnWidth(kColId,   200);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    tableLayout->addWidget(m_table, 1);

    // ── Action buttons row ────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();

    m_installBtn = new QPushButton(tr("Install…"), tableBox);
    m_installBtn->setToolTip(
        tr("Install a plugin from a .zip archive.\n"
           "The archive is extracted to the plugins directory and loaded immediately."));
    btnRow->addWidget(m_installBtn);

    m_uninstallBtn = new QPushButton(tr("Uninstall"), tableBox);
    m_uninstallBtn->setToolTip(tr("Remove the selected plugin's files from the plugins directory."));
    m_uninstallBtn->setEnabled(false);
    btnRow->addWidget(m_uninstallBtn);

    m_enableBtn = new QPushButton(tr("Enable"), tableBox);
    m_enableBtn->setToolTip(tr("Load the selected plugin into this session."));
    m_enableBtn->setEnabled(false);
    btnRow->addWidget(m_enableBtn);

    m_disableBtn = new QPushButton(tr("Disable"), tableBox);
    m_disableBtn->setToolTip(tr("Unload the selected plugin from this session (keeps the file)."));
    m_disableBtn->setEnabled(false);
    btnRow->addWidget(m_disableBtn);

    btnRow->addStretch();

    m_refreshBtn = new QPushButton(tr("Refresh"), tableBox);
    m_refreshBtn->setToolTip(tr("Rescan the plugins directory."));
    btnRow->addWidget(m_refreshBtn);

    tableLayout->addLayout(btnRow);
    splitter->addWidget(tableBox);

    // ── Details pane ──────────────────────────────────────────────────────
    auto* detailsBox    = new QGroupBox(tr("Plugin Details"), splitter);
    auto* detailsOuter  = new QVBoxLayout(detailsBox);

    auto* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight);

    m_detailName    = new QLabel(detailsBox);
    m_detailId      = new QLabel(detailsBox);
    m_detailVersion = new QLabel(detailsBox);
    m_detailAuthor  = new QLabel(detailsBox);
    m_detailType    = new QLabel(detailsBox);
    m_detailStatus  = new QLabel(detailsBox);
    m_detailLicense = new QLabel(detailsBox);
    m_detailPath    = new QLabel(detailsBox);
    m_detailPath->setWordWrap(true);
    m_detailPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_detailDesc    = new QLabel(detailsBox);
    m_detailDesc->setWordWrap(true);

    formLayout->addRow(tr("Name:"),     m_detailName);
    formLayout->addRow(tr("ID:"),       m_detailId);
    formLayout->addRow(tr("Version:"),  m_detailVersion);
    formLayout->addRow(tr("Author:"),   m_detailAuthor);
    formLayout->addRow(tr("Type:"),     m_detailType);
    formLayout->addRow(tr("Status:"),   m_detailStatus);
    formLayout->addRow(tr("License:"),  m_detailLicense);
    formLayout->addRow(tr("Location:"), m_detailPath);
    formLayout->addRow(tr("Description:"), m_detailDesc);
    detailsOuter->addLayout(formLayout);

    // Auto-load toggle
    m_autoLoadCheck = new QCheckBox(tr("Auto-load this plugin on startup"), detailsBox);
    m_autoLoadCheck->setEnabled(false);
    detailsOuter->addWidget(m_autoLoadCheck);

    // License key row
    auto* licRow = new QHBoxLayout();
    licRow->addWidget(new QLabel(tr("License key:"), detailsBox));
    m_licenseEdit = new QLineEdit(detailsBox);
    m_licenseEdit->setEchoMode(QLineEdit::Password);
    m_licenseEdit->setPlaceholderText(tr("Enter license key…"));
    m_licenseEdit->setEnabled(false);
    licRow->addWidget(m_licenseEdit, 1);
    m_licenseBtn = new QPushButton(tr("Set License"), detailsBox);
    m_licenseBtn->setEnabled(false);
    licRow->addWidget(m_licenseBtn);
    detailsOuter->addLayout(licRow);

    splitter->addWidget(detailsBox);
    splitter->setSizes({320, 240});

    // ── Status label + Close ──────────────────────────────────────────────
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    root->addWidget(m_statusLabel);

    auto* closeBB = new QDialogButtonBox(QDialogButtonBox::Close, this);
    root->addWidget(closeBB);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_installBtn,   &QPushButton::clicked,  this, &PluginManagerDialog::OnInstall);
    connect(m_uninstallBtn, &QPushButton::clicked,  this, &PluginManagerDialog::OnUninstall);
    connect(m_enableBtn,    &QPushButton::clicked,  this, &PluginManagerDialog::OnEnable);
    connect(m_disableBtn,   &QPushButton::clicked,  this, &PluginManagerDialog::OnDisable);
    connect(m_refreshBtn,   &QPushButton::clicked,  this, &PluginManagerDialog::OnRefresh);
    connect(m_autoLoadCheck,&QCheckBox::toggled,    this, &PluginManagerDialog::OnAutoLoadToggled);
    connect(m_licenseBtn,   &QPushButton::clicked,  this, &PluginManagerDialog::OnSetLicense);
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

    auto& mgr             = plugin::PluginManager::GetInstance();
    const auto discovered = mgr.DiscoverPlugins();
    const auto& loaded    = mgr.GetLoadedPlugins();

    // Index loaded plugins by path so we can mark discovered-but-unloaded ones.
    std::map<std::filesystem::path, const plugin::PluginLoadInfo*> byPath;
    for (const auto& [id, info] : loaded)
        byPath[info.path] = &info;

    // Rows: loaded first, then discovered-but-not-loaded.
    std::vector<std::pair<std::filesystem::path, const plugin::PluginLoadInfo*>> rows;
    for (const auto& [id, info] : loaded)
        rows.emplace_back(info.path, &info);
    for (const auto& p : discovered)
        if (byPath.find(p) == byPath.end())
            rows.emplace_back(p, nullptr);

    m_table->setRowCount(static_cast<int>(rows.size()));

    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        const auto& [path, info] = rows[static_cast<size_t>(i)];
        const bool isLoaded = (info != nullptr);

        plugin::PluginMetadata meta;
        int statusVal = static_cast<int>(plugin::PluginStatus::Unloaded);
        if (isLoaded && info->instance)
        {
            meta      = info->instance->GetMetadata();
            statusVal = static_cast<int>(info->instance->GetStatus());
        }
        else
        {
            meta.name = path.stem().string();
            meta.id   = meta.name;
        }

        auto* nameItem = new QTableWidgetItem(QString::fromStdString(meta.name));
        nameItem->setData(Qt::UserRole, QString::fromStdString(meta.id));
        m_table->setItem(i, kColName,    nameItem);
        m_table->setItem(i, kColId,      new QTableWidgetItem(QString::fromStdString(meta.id)));
        m_table->setItem(i, kColVersion, new QTableWidgetItem(QString::fromStdString(meta.version)));
        m_table->setItem(i, kColType,    new QTableWidgetItem(TypeName(static_cast<int>(meta.type))));

        const QString statusStr = StatusText(statusVal);
        auto* statusItem = new QTableWidgetItem(statusStr);
        const bool isActive = (statusVal == static_cast<int>(plugin::PluginStatus::Active) ||
                               statusVal == static_cast<int>(plugin::PluginStatus::Initialized));
        statusItem->setForeground(isActive    ? QColor(Qt::darkGreen) :
                                  !isLoaded   ? QColor(Qt::gray) :
                                  statusVal == static_cast<int>(plugin::PluginStatus::Error)
                                              ? QColor(Qt::red)
                                              : QColor(Qt::black));
        m_table->setItem(i, kColStatus, statusItem);

        const bool autoLoad = isLoaded && info->autoLoad;
        m_table->setItem(i, kColAutoLoad,
            new QTableWidgetItem(autoLoad ? tr("Yes") : tr("No")));
    }

    m_table->setSortingEnabled(true);
    m_statusLabel->setText(
        tr("%1 plugin(s) discovered, %2 loaded")
            .arg(rows.size())
            .arg(loaded.size()));

    ClearDetails();
    OnSelectionChanged();
}

// ---------------------------------------------------------------------------
// Details pane
// ---------------------------------------------------------------------------

void PluginManagerDialog::UpdateDetails()
{
    const int row = m_table->currentRow();
    if (row < 0) { ClearDetails(); return; }

    const QString id = m_table->item(row, kColName)
                           ->data(Qt::UserRole).toString();

    auto& mgr = plugin::PluginManager::GetInstance();
    const auto& loaded = mgr.GetLoadedPlugins();
    const auto it = loaded.find(id.toStdString());

    if (it == loaded.end() || !it->second.instance)
    {
        // Discovered but not loaded — show minimal info from the path column.
        ClearDetails();
        m_detailName->setText(m_table->item(row, kColName)->text());
        m_detailId->setText(m_table->item(row, kColId)->text());
        m_detailStatus->setText(tr("Not loaded"));
        return;
    }

    const plugin::PluginLoadInfo& info = it->second;
    const plugin::PluginMetadata  meta = info.instance->GetMetadata();
    const plugin::PluginStatus    stat = info.instance->GetStatus();

    m_detailName->setText(QString::fromStdString(meta.name));
    m_detailId->setText(QString::fromStdString(meta.id));
    m_detailVersion->setText(QString::fromStdString(meta.version));
    m_detailAuthor->setText(QString::fromStdString(meta.author));
    m_detailType->setText(TypeName(static_cast<int>(meta.type)));
    m_detailDesc->setText(QString::fromStdString(meta.description));
    m_detailPath->setText(QString::fromStdString(info.path.string()));

    // Status — show error message when applicable.
    QString statusStr = StatusText(static_cast<int>(stat));
    if (stat == plugin::PluginStatus::Error)
        statusStr += tr(": %1").arg(
            QString::fromStdString(info.instance->GetLastError()));
    m_detailStatus->setText(statusStr);

    // License
    if (meta.requiresLicense)
    {
        m_detailLicense->setText(
            info.instance->IsLicensed() ? tr("Required — licensed ✓")
                                        : tr("Required — NOT licensed"));
        m_licenseEdit->setEnabled(true);
        m_licenseBtn->setEnabled(true);
    }
    else
    {
        m_detailLicense->setText(tr("Not required"));
        m_licenseEdit->setEnabled(false);
        m_licenseBtn->setEnabled(false);
    }

    // Auto-load — block signal so toggling the checkbox doesn't fire OnAutoLoadToggled.
    m_autoLoadCheck->blockSignals(true);
    m_autoLoadCheck->setChecked(info.autoLoad);
    m_autoLoadCheck->blockSignals(false);
    m_autoLoadCheck->setEnabled(true);
}

void PluginManagerDialog::ClearDetails()
{
    for (auto* lbl : {m_detailName, m_detailId, m_detailVersion, m_detailAuthor,
                      m_detailType, m_detailStatus, m_detailLicense,
                      m_detailPath, m_detailDesc})
        lbl->clear();

    m_autoLoadCheck->blockSignals(true);
    m_autoLoadCheck->setChecked(false);
    m_autoLoadCheck->blockSignals(false);
    m_autoLoadCheck->setEnabled(false);
    m_licenseEdit->clear();
    m_licenseEdit->setEnabled(false);
    m_licenseBtn->setEnabled(false);
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
        tr("Plugin archives (*.zip);; All files (*.*)"));
    if (path.isEmpty()) return;

    auto& mgr = plugin::PluginManager::GetInstance();
    auto result = mgr.RegisterPlugin(std::filesystem::path(path.toStdString()));
    if (!result.isOk())
    {
        QMessageBox::critical(this, tr("Install Plugin"),
            tr("Failed to install plugin:\n%1")
                .arg(QString::fromStdString(result.error().what())));
        return;
    }

    const std::string pluginId = result.unwrap();
    util::Logger::Info("[PluginManager] Installed and loaded plugin '{}'", pluginId);
    m_statusLabel->setText(tr("Installed: %1").arg(QString::fromStdString(pluginId)));
    PopulateTable();
}

void PluginManagerDialog::OnUninstall()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    const QString id = m_table->item(row, kColName)->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    const auto answer = QMessageBox::question(
        this, tr("Uninstall Plugin"),
        tr("Uninstall plugin \"%1\"?\n"
           "The plugin file will be removed from the plugins directory.").arg(id),
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
    const QString id = m_table->item(row, kColName)->data(Qt::UserRole).toString();

    const auto result = plugin::PluginManager::GetInstance().EnablePlugin(id.toStdString());
    if (!result.isOk())
    {
        QMessageBox::critical(this, tr("Enable Plugin"),
            tr("Failed to enable \"%1\":\n%2")
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
    const QString id = m_table->item(row, kColName)->data(Qt::UserRole).toString();

    const auto result = plugin::PluginManager::GetInstance().DisablePlugin(id.toStdString());
    if (!result.isOk())
    {
        QMessageBox::critical(this, tr("Disable Plugin"),
            tr("Failed to disable \"%1\":\n%2")
                .arg(id)
                .arg(QString::fromStdString(result.error().what())));
        return;
    }

    util::Logger::Info("[PluginManager] Disabled plugin '{}'", id.toStdString());
    m_statusLabel->setText(tr("Disabled: %1").arg(id));
    PopulateTable();
}

void PluginManagerDialog::OnAutoLoadToggled(bool checked)
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    const QString id = m_table->item(row, kColName)->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    plugin::PluginManager::GetInstance().SetPluginAutoLoad(id.toStdString(), checked);
    util::Logger::Info("[PluginManager] Plugin '{}' auto-load = {}", id.toStdString(), checked);

    // Refresh just the Auto-load column cell without full repopulation.
    if (m_table->item(row, kColAutoLoad))
        m_table->item(row, kColAutoLoad)->setText(checked ? tr("Yes") : tr("No"));
}

void PluginManagerDialog::OnSetLicense()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    const QString id  = m_table->item(row, kColName)->data(Qt::UserRole).toString();
    const QString key = m_licenseEdit->text().trimmed();

    if (key.isEmpty())
    {
        QMessageBox::warning(this, tr("Set License"), tr("Please enter a license key."));
        return;
    }

    auto* plugin = plugin::PluginManager::GetInstance().GetPlugin(id.toStdString());
    if (!plugin) return;

    if (plugin->SetLicense(key.toStdString()))
    {
        QMessageBox::information(this, tr("Set License"), tr("License key accepted."));
        m_licenseEdit->clear();
        UpdateDetails();
    }
    else
    {
        QMessageBox::critical(this, tr("Set License"),
            tr("License key rejected:\n%1")
                .arg(QString::fromStdString(plugin->GetLastError())));
    }
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
    {
        const auto stat = m_table->item(row, kColStatus)->text();
        isLoaded = (stat != tr("Not loaded") && stat != tr("Unloaded"));
    }

    m_uninstallBtn->setEnabled(sel);
    m_enableBtn->setEnabled(sel && !isLoaded);
    m_disableBtn->setEnabled(sel && isLoaded);

    UpdateDetails();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString PluginManagerDialog::StatusText(int status) const
{
    using PS = plugin::PluginStatus;
    switch (static_cast<PS>(status))
    {
        case PS::Unloaded:    return tr("Not loaded");
        case PS::Loaded:      return tr("Loaded");
        case PS::Initialized: return tr("Initialized");
        case PS::Active:      return tr("Active");
        case PS::Error:       return tr("Error");
        case PS::Disabled:    return tr("Disabled");
        default:              return tr("Unknown");
    }
}

QString PluginManagerDialog::TypeName(int type) const
{
    using PT = plugin::PluginType;
    switch (static_cast<PT>(type))
    {
        case PT::Parser:          return tr("Parser");
        case PT::Filter:          return tr("Filter");
        case PT::FieldConversion: return tr("Field Conversion");
        case PT::Exporter:        return tr("Exporter");
        case PT::Analyzer:        return tr("Analyzer");
        case PT::AIProvider:      return tr("AI Provider");
        case PT::Connector:       return tr("Connector");
        case PT::Visualizer:      return tr("Visualizer");
        case PT::Custom:          return tr("Custom");
        default:                  return tr("Unknown");
    }
}

} // namespace ui::qt
