/**
 * @file FilterProfilesPanel.cpp
 * @brief Implementation of FilterProfilesPanel and FilterProfile JSON round-trip.
 * @author LogViewer Development Team
 * @date 2026
 */
#include "FilterProfilesPanel.hpp"

#include "Config.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <filesystem>
#include <fstream>

namespace ui::qt {

// ---------------------------------------------------------------------------
// FilterProfile JSON round-trip
// ---------------------------------------------------------------------------

nlohmann::json FilterProfile::ToJson() const
{
    nlohmann::json j;
    j["name"] = name;

    j["timeRange"] = {
        {"field",  timeRange.field},
        {"from",   timeRange.from},
        {"to",     timeRange.to},
        {"active", timeRange.active}
    };

    j["uncheckedActors"] = nlohmann::json::array();
    for (const auto& a : uncheckedActors)
        j["uncheckedActors"].push_back(a);

    j["hasTypeFilter"] = hasTypeFilter;
    j["checkedTypes"] = nlohmann::json::array();
    for (const auto& t : checkedTypes)
        j["checkedTypes"].push_back(t);

    return j;
}

FilterProfile FilterProfile::FromJson(const nlohmann::json& j)
{
    FilterProfile fp;
    fp.name = j.value("name", "");

    if (j.contains("timeRange") && j.at("timeRange").is_object())
    {
        const auto& tr = j.at("timeRange");
        fp.timeRange.field  = tr.value("field",  "timestamp");
        fp.timeRange.from   = tr.value("from",   "");
        fp.timeRange.to     = tr.value("to",     "");
        fp.timeRange.active = tr.value("active", false);
    }

    if (j.contains("uncheckedActors") && j.at("uncheckedActors").is_array())
        for (const auto& a : j.at("uncheckedActors"))
            if (a.is_string()) fp.uncheckedActors.push_back(a.get<std::string>());

    fp.hasTypeFilter = j.value("hasTypeFilter", false);
    if (j.contains("checkedTypes") && j.at("checkedTypes").is_array())
        for (const auto& t : j.at("checkedTypes"))
            if (t.is_string()) fp.checkedTypes.push_back(t.get<std::string>());

    return fp;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

FilterProfilesPanel::FilterProfilesPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildLayout();
    LoadFromFile();
}

void FilterProfilesPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* infoLabel = new QLabel(
        tr("Save and restore named filter states.\n"
           "A profile captures the current time range, actor check\n"
           "state, and type filter selections."),
        this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // ── Profile list ──────────────────────────────────────────────────────
    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    // ── Name input ────────────────────────────────────────────────────────
    auto* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel(tr("Name:"), this));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Profile name…"));
    m_nameEdit->setToolTip(tr("Name for the profile to save"));
    nameRow->addWidget(m_nameEdit, 1);
    layout->addLayout(nameRow);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto* btnRow  = new QHBoxLayout();
    m_saveBtn   = new QPushButton(tr("Save current state"), this);
    m_loadBtn   = new QPushButton(tr("Load"),   this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    m_loadBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_saveBtn->setToolTip(
        tr("Capture the current time range, actor, and type filters as a new profile"));
    m_loadBtn->setToolTip(tr("Apply the selected profile to all filter panels"));
    m_deleteBtn->setToolTip(tr("Remove the selected profile from the list"));
    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_loadBtn);
    btnRow->addWidget(m_deleteBtn);
    layout->addLayout(btnRow);

    // ── Status ────────────────────────────────────────────────────────────
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("font-style: italic;");
    layout->addWidget(m_statusLabel);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_list, &QListWidget::itemSelectionChanged,
            this, &FilterProfilesPanel::HandleSelectionChanged);
    connect(m_list, &QListWidget::itemDoubleClicked,
            this, &FilterProfilesPanel::HandleLoad);
    connect(m_saveBtn,   &QPushButton::clicked, this, &FilterProfilesPanel::HandleSave);
    connect(m_loadBtn,   &QPushButton::clicked, this, &FilterProfilesPanel::HandleLoad);
    connect(m_deleteBtn, &QPushButton::clicked, this, &FilterProfilesPanel::HandleDelete);

    // When user clicks in list, copy the profile name to the name edit
    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem*)
            {
                if (current)
                    m_nameEdit->setText(current->text());
            });
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void FilterProfilesPanel::StoreProfile(const FilterProfile& profile)
{
    // Replace if name already exists, otherwise append
    for (auto& existing : m_profiles)
    {
        if (existing.name == profile.name)
        {
            existing = profile;
            RebuildList();
            SaveToFile();
            m_statusLabel->setText(
                tr("Profile \"%1\" updated.")
                    .arg(QString::fromStdString(profile.name)));
            return;
        }
    }
    m_profiles.push_back(profile);
    RebuildList();
    SaveToFile();
    m_statusLabel->setText(
        tr("Profile \"%1\" saved.")
            .arg(QString::fromStdString(profile.name)));
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void FilterProfilesPanel::HandleSave()
{
    QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty())
    {
        QMessageBox::warning(this, tr("Save Profile"),
                             tr("Please enter a profile name."));
        return;
    }

    // Check for overwrite
    for (const auto& p : m_profiles)
    {
        if (p.name == name.toStdString())
        {
            const auto btn = QMessageBox::question(
                this, tr("Overwrite?"),
                tr("A profile named \"%1\" already exists. Overwrite it?").arg(name));
            if (btn != QMessageBox::Yes) return;
            break;
        }
    }

    emit SaveRequested(name);
}

void FilterProfilesPanel::HandleLoad()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= static_cast<int>(m_profiles.size())) return;
    emit ProfileLoadRequested(m_profiles[static_cast<size_t>(row)]);
    m_statusLabel->setText(
        tr("Profile \"%1\" loaded.")
            .arg(QString::fromStdString(m_profiles[static_cast<size_t>(row)].name)));
}

void FilterProfilesPanel::HandleDelete()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= static_cast<int>(m_profiles.size())) return;

    const QString name =
        QString::fromStdString(m_profiles[static_cast<size_t>(row)].name);
    if (QMessageBox::question(this, tr("Delete Profile"),
            tr("Delete profile \"%1\"?").arg(name)) != QMessageBox::Yes)
        return;

    m_profiles.erase(m_profiles.begin() + row);
    RebuildList();
    SaveToFile();
    m_statusLabel->setText(tr("Profile \"%1\" deleted.").arg(name));
}

void FilterProfilesPanel::HandleSelectionChanged()
{
    const bool has = (m_list->currentRow() >= 0);
    m_loadBtn->setEnabled(has);
    m_deleteBtn->setEnabled(has);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void FilterProfilesPanel::RebuildList()
{
    m_list->clear();
    for (const auto& p : m_profiles)
    {
        QString label = QString::fromStdString(p.name);
        QStringList details;
        if (p.timeRange.active || !p.timeRange.from.empty() || !p.timeRange.to.empty())
            details << tr("time");
        if (!p.uncheckedActors.empty())
            details << tr("actors");
        if (p.hasTypeFilter && !p.checkedTypes.empty())
            details << tr("types");
        if (!details.isEmpty())
            label += QString(" [%1]").arg(details.join(", "));
        m_list->addItem(label);
    }
    HandleSelectionChanged();
}

void FilterProfilesPanel::SaveToFile()
{
    try
    {
        const std::string path = DefaultFilePath();
        std::filesystem::create_directories(
            std::filesystem::path(path).parent_path());

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& p : m_profiles)
            arr.push_back(p.ToJson());

        std::ofstream ofs(path);
        if (!ofs) throw std::runtime_error("cannot open for writing");
        ofs << arr.dump(2);
    }
    catch (const std::exception& e)
    {
        m_statusLabel->setText(
            tr("Save error: %1").arg(QString::fromStdString(e.what())));
    }
}

void FilterProfilesPanel::LoadFromFile()
{
    try
    {
        const std::string path = DefaultFilePath();
        std::ifstream ifs(path);
        if (!ifs.is_open()) return; // no file yet — normal on first run

        nlohmann::json j;
        ifs >> j;
        if (!j.is_array()) return;

        m_profiles.clear();
        for (const auto& item : j)
        {
            try { m_profiles.push_back(FilterProfile::FromJson(item)); }
            catch (...) {}
        }
        RebuildList();
        if (!m_profiles.empty())
            m_statusLabel->setText(
                tr("Loaded %1 profile(s).").arg(m_profiles.size()));
    }
    catch (...) {} // silently ignore missing / corrupt file
}

std::string FilterProfilesPanel::DefaultFilePath()
{
    return (config::GetConfig().GetDefaultAppPath() / "filter_profiles.json").string();
}

} // namespace ui::qt
