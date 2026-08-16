/**
 * @file FilterProfilesPanel.cpp
 * @brief Implementation of FilterProfilesPanel and FilterProfile JSON round-trip.
 * @author LogViewer Development Team
 * @date 2026
 */
#include "FilterProfilesPanel.hpp"

#include "Config.hpp"
#include "Logger.hpp"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>

namespace ui::qt {

namespace {

/**
 * @brief Parses a JSON array of profile entries, rejecting entries with an
 *        empty name or a name already present in @p existingNames.
 *
 * Shared by LoadFromFile() (existingNames starts empty — dedup only within
 * the file) and HandleImport() (existingNames pre-seeded with the panel's
 * current profile names, so imported profiles that collide are reported
 * back to the caller instead of silently dropped).
 *
 * @param j Parsed JSON value; must be an array to yield any results.
 * @param existingNames Names already taken; updated with each accepted name.
 * @param[out] parseErrors Count of entries that failed to parse as a profile.
 * @param[out] rejectedNames Names skipped due to being empty or a duplicate.
 * @return Successfully parsed, uniquely-named profiles, in file order.
 */
std::vector<FilterProfile> ParseProfilesArray(
    const nlohmann::json& j, std::set<std::string>& existingNames,
    size_t& parseErrors, std::vector<std::string>& rejectedNames)
{
    std::vector<FilterProfile> result;
    if (!j.is_array())
        return result;

    static constexpr size_t kLargeProfileCountWarningThreshold = 500;
    if (j.size() > kLargeProfileCountWarningThreshold)
        util::Logger::Warn("[FilterProfiles] Profiles array has an unusually large entry count ({})",
            j.size());

    for (const auto& item : j)
    {
        FilterProfile profile;
        try { profile = FilterProfile::FromJson(item); }
        catch (...) { ++parseErrors; continue; }

        if (profile.name.empty())
        {
            util::Logger::Warn("[FilterProfiles] Skipping profile entry with empty name");
            rejectedNames.emplace_back("(empty name)");
            continue;
        }
        if (!existingNames.insert(profile.name).second)
        {
            util::Logger::Warn("[FilterProfiles] Skipping duplicate profile name '{}'", profile.name);
            rejectedNames.push_back(profile.name);
            continue;
        }

        result.push_back(std::move(profile));
    }
    return result;
}

} // namespace

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
    m_list->setObjectName("filterProfilesList");
    m_list->setAlternatingRowColors(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);

    // ── Name input ────────────────────────────────────────────────────────
    auto* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel(tr("Name:"), this));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName("filterProfileNameEdit");
    m_nameEdit->setPlaceholderText(tr("Profile name…"));
    m_nameEdit->setToolTip(tr("Name for the profile to save"));
    nameRow->addWidget(m_nameEdit, 1);
    layout->addLayout(nameRow);

    // ── Buttons ───────────────────────────────────────────────────────────
    // Object names below let tests locate these buttons via findChild<>()
    // and drive them with QTest::mouseClick() instead of OS-level UI
    // automation (which can't distinguish this window from any other).
    auto* btnRow  = new QHBoxLayout();
    m_saveBtn   = new QPushButton(tr("Save current state"), this);
    m_loadBtn   = new QPushButton(tr("Load"),   this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    m_saveBtn->setObjectName("filterProfileSaveButton");
    m_loadBtn->setObjectName("filterProfileLoadButton");
    m_deleteBtn->setObjectName("filterProfileDeleteButton");
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

    // ── Export / Import (sharing with teammates) ────────────────────────────
    auto* shareRow = new QHBoxLayout();
    m_exportBtn = new QPushButton(tr("Export…"), this);
    m_importBtn = new QPushButton(tr("Import…"), this);
    m_exportBtn->setObjectName("filterProfileExportButton");
    m_importBtn->setObjectName("filterProfileImportButton");
    m_exportBtn->setEnabled(false);
    m_exportBtn->setToolTip(tr("Save the selected profile to a .filters.json file"));
    m_importBtn->setToolTip(tr("Load profiles from a .filters.json file"));
    shareRow->addWidget(m_exportBtn);
    shareRow->addWidget(m_importBtn);
    layout->addLayout(shareRow);

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
    connect(m_exportBtn, &QPushButton::clicked, this, &FilterProfilesPanel::HandleExport);
    connect(m_importBtn, &QPushButton::clicked, this, &FilterProfilesPanel::HandleImport);

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
            util::Logger::Debug("[FilterProfiles] Updating existing profile '{}'", profile.name);
            existing = profile;
            RebuildList();
            SaveToFile();
            util::Logger::Info("[FilterProfiles] Profile '{}' updated", profile.name);
            m_statusLabel->setText(
                tr("Profile \"%1\" updated.")
                    .arg(QString::fromStdString(profile.name)));
            return;
        }
    }
    util::Logger::Debug("[FilterProfiles] Storing new profile '{}'", profile.name);
    m_profiles.push_back(profile);
    RebuildList();
    SaveToFile();
    util::Logger::Info("[FilterProfiles] Profile '{}' saved", profile.name);
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
        util::Logger::Warn("[FilterProfiles] Save requested with empty profile name");
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
    const auto& profile = m_profiles[static_cast<size_t>(row)];
    util::Logger::Debug("[FilterProfiles] Loading profile '{}'", profile.name);
    emit ProfileLoadRequested(profile);
    util::Logger::Info("[FilterProfiles] Profile '{}' loaded", profile.name);
    m_statusLabel->setText(
        tr("Profile \"%1\" loaded.")
            .arg(QString::fromStdString(profile.name)));
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

    util::Logger::Debug("[FilterProfiles] Deleting profile '{}'", name.toStdString());
    m_profiles.erase(m_profiles.begin() + row);
    RebuildList();
    SaveToFile();
    util::Logger::Info("[FilterProfiles] Profile '{}' deleted", name.toStdString());
    m_statusLabel->setText(tr("Profile \"%1\" deleted.").arg(name));
}

void FilterProfilesPanel::HandleSelectionChanged()
{
    const bool has = (m_list->currentRow() >= 0);
    m_loadBtn->setEnabled(has);
    m_deleteBtn->setEnabled(has);
    m_exportBtn->setEnabled(has);
}

void FilterProfilesPanel::HandleExport()
{
    const int row = m_list->currentRow();
    if (row < 0 || row >= static_cast<int>(m_profiles.size())) return;
    const auto& profile = m_profiles[static_cast<size_t>(row)];

    const QString suggested = QString::fromStdString(profile.name) + ".filters.json";
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Filter Profile"),
        suggested, tr("Filter Profiles (*.filters.json);;All Files (*)"));
    if (path.isEmpty()) return;

    const auto result = ExportProfileToPath(profile, path);
    if (result.isOk())
    {
        m_statusLabel->setText(
            tr("Exported \"%1\" to %2.")
                .arg(QString::fromStdString(profile.name), path));
    }
    else
    {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Could not write to %1:\n%2").arg(path, QString::fromStdString(result.error().what())));
    }
}

void FilterProfilesPanel::HandleImport()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Filter Profiles"),
        QString(), tr("Filter Profiles (*.filters.json);;All Files (*)"));
    if (path.isEmpty()) return;

    auto readResult = ReadProfilesFile(path);
    if (!readResult.isOk())
    {
        QMessageBox::warning(this, tr("Import Failed"),
            tr("Could not read %1:\n%2").arg(path, QString::fromStdString(readResult.error().what())));
        return;
    }
    auto incoming = readResult.unwrap();

    // Ask once, up front, whether any name collisions should be overwritten —
    // MergeProfiles() itself makes no UI decisions.
    bool overwriteCollisions = false;
    std::vector<std::string> collisions;
    for (const auto& p : incoming)
        if (HasProfile(p.name))
            collisions.push_back(p.name);

    if (!collisions.empty())
    {
        std::string names;
        for (const auto& n : collisions) names += n + "\n";
        const auto btn = QMessageBox::question(this, tr("Overwrite Existing Profiles?"),
            tr("%1 imported profile(s) have the same name as an existing profile:\n\n%2\n\nOverwrite them?")
                .arg(collisions.size())
                .arg(QString::fromStdString(names)));
        overwriteCollisions = (btn == QMessageBox::Yes);
    }

    const size_t skippedBefore = collisions.size();
    const size_t applied = MergeProfiles(std::move(incoming), overwriteCollisions);
    const size_t skipped = overwriteCollisions ? 0 : skippedBefore;

    m_statusLabel->setText(
        tr("Imported %1 profile(s)%2.")
            .arg(applied)
            .arg(skipped == 0 ? QString() : tr(" (%1 skipped)").arg(skipped)));
}

// ---------------------------------------------------------------------------
// Public: Export / Import (testable without a native file dialog)
// ---------------------------------------------------------------------------

FilterProfilesPanel::ExportResult FilterProfilesPanel::ExportProfileToPath(
    const FilterProfile& profile, const QString& path)
{
    try
    {
        nlohmann::json arr = nlohmann::json::array();
        arr.push_back(profile.ToJson());

        std::ofstream ofs(path.toStdString());
        if (!ofs)
            return ExportResult::Err(error::Error(error::ErrorCode::IOError,
                "cannot open for writing", /*showMsgBox=*/false));
        ofs << arr.dump(2);

        util::Logger::Info("[FilterProfiles] Exported profile '{}' to '{}'",
            profile.name, path.toStdString());
        return ExportResult::Ok({});
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[FilterProfiles] Failed to export profile: {}", e.what());
        return ExportResult::Err(error::Error(error::ErrorCode::IOError, e.what(), /*showMsgBox=*/false));
    }
}

FilterProfilesPanel::ReadProfilesResult FilterProfilesPanel::ReadProfilesFile(const QString& path)
{
    nlohmann::json j;
    try
    {
        std::ifstream ifs(path.toStdString());
        if (!ifs.is_open())
            return ReadProfilesResult::Err(error::Error(error::ErrorCode::IOError,
                "cannot open file", /*showMsgBox=*/false));
        ifs >> j;
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[FilterProfiles] Failed to read import file: {}", e.what());
        return ReadProfilesResult::Err(error::Error(error::ErrorCode::IOError, e.what(), /*showMsgBox=*/false));
    }
    if (!j.is_array())
        return ReadProfilesResult::Err(error::Error(error::ErrorCode::InvalidArgument,
            "file does not contain a filter profiles array", /*showMsgBox=*/false));

    std::set<std::string> seenNames;
    size_t parseErrors = 0;
    std::vector<std::string> rejected;
    auto profiles = ParseProfilesArray(j, seenNames, parseErrors, rejected);

    if (parseErrors > 0)
        util::Logger::Warn("[FilterProfiles] {} profile entry/entries in '{}' could not be parsed",
            parseErrors, path.toStdString());
    if (!rejected.empty())
        util::Logger::Warn("[FilterProfiles] {} profile entry/entries in '{}' rejected (empty/duplicate name)",
            rejected.size(), path.toStdString());

    return ReadProfilesResult::Ok(std::move(profiles));
}

size_t FilterProfilesPanel::MergeProfiles(std::vector<FilterProfile> incoming, bool overwriteCollisions)
{
    size_t applied = 0;
    for (auto& p : incoming)
    {
        auto existing = std::find_if(m_profiles.begin(), m_profiles.end(),
            [&](const FilterProfile& e) { return e.name == p.name; });
        if (existing != m_profiles.end())
        {
            if (!overwriteCollisions) continue;
            *existing = std::move(p);
            ++applied;
        }
        else
        {
            m_profiles.push_back(std::move(p));
            ++applied;
        }
    }

    if (applied > 0)
    {
        RebuildList();
        SaveToFile();
    }
    util::Logger::Info("[FilterProfiles] Merged {} profile(s) ({} of {} incoming applied)",
        applied, applied, incoming.size());
    return applied;
}

bool FilterProfilesPanel::HasProfile(const std::string& name) const
{
    return std::any_of(m_profiles.begin(), m_profiles.end(),
        [&](const FilterProfile& p) { return p.name == name; });
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
        util::Logger::Debug("[FilterProfiles] Saving {} profile(s) to '{}'",
            m_profiles.size(), path);
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
        util::Logger::Error("[FilterProfiles] Failed to save profiles to file: {}", e.what());
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
        if (!ifs.is_open())
        {
            util::Logger::Debug("[FilterProfiles] No profiles file found at '{}' (first run)", path);
            return; // no file yet — normal on first run
        }

        util::Logger::Debug("[FilterProfiles] Loading profiles from '{}'", path);
        nlohmann::json j;
        ifs >> j;
        if (!j.is_array())
        {
            util::Logger::Warn("[FilterProfiles] Profiles file '{}' does not contain a JSON array", path);
            return;
        }

        // Name is the unique identifier — entries with no name (would
        // collide with each other) or a name already seen in this file
        // (last-writer-wins would otherwise silently drop one profile
        // without any indication to the user) are rejected.
        std::set<std::string> seenNames;
        size_t parseErrors = 0;
        std::vector<std::string> rejected;
        m_profiles = ParseProfilesArray(j, seenNames, parseErrors, rejected);

        if (parseErrors > 0)
            util::Logger::Warn("[FilterProfiles] {} profile entry/entries could not be parsed", parseErrors);
        if (!rejected.empty())
            util::Logger::Warn("[FilterProfiles] {} profile entry/entries rejected (empty/duplicate name)", rejected.size());
        RebuildList();
        if (!m_profiles.empty())
        {
            util::Logger::Info("[FilterProfiles] Loaded {} profile(s) from file", m_profiles.size());
            m_statusLabel->setText(
                tr("Loaded %1 profile(s).").arg(m_profiles.size()));
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[FilterProfiles] Error loading profiles file: {}", e.what());
    }
    catch (...) {} // silently ignore remaining unknown errors
}

std::string FilterProfilesPanel::DefaultFilePath()
{
    return (config::GetConfig().GetDefaultAppPath() / "filter_profiles.json").string();
}

} // namespace ui::qt
