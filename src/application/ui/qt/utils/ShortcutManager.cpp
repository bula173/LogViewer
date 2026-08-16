#include "ShortcutManager.hpp"

#include "Config.hpp"
#include "Logger.hpp"

#include <QAction>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace ui::qt {

ShortcutManager& ShortcutManager::getInstance()
{
    static ShortcutManager instance;
    return instance;
}

ShortcutManager::ShortcutManager()
{
    LoadOverrides();
}

void ShortcutManager::Register(const std::string& id, const QString& category,
    const QString& label, QAction* action)
{
    if (!action) return;

    const auto existing = std::find_if(m_entries.begin(), m_entries.end(),
        [&](const Entry& e) { return e.id == id; });
    if (existing != m_entries.end())
    {
        existing->action = action;
        return;
    }

    Entry entry;
    entry.id = id;
    entry.category = category;
    entry.label = label;
    entry.action = action;
    entry.defaultSequence = action->shortcut();
    m_entries.push_back(entry);

    const auto pending = std::find_if(m_pendingOverrides.begin(), m_pendingOverrides.end(),
        [&](const auto& kv) { return kv.first == id; });
    if (pending != m_pendingOverrides.end())
        action->setShortcut(pending->second);
}

std::vector<ShortcutInfo> ShortcutManager::GetAll() const
{
    std::vector<ShortcutInfo> result;
    result.reserve(m_entries.size());
    for (const auto& e : m_entries)
    {
        result.push_back(ShortcutInfo{
            e.id, e.category, e.label, e.action->shortcut(), e.defaultSequence});
    }
    return result;
}

ShortcutResult ShortcutManager::SetShortcut(const std::string& id, const QKeySequence& sequence)
{
    const auto target = std::find_if(m_entries.begin(), m_entries.end(),
        [&](const Entry& e) { return e.id == id; });
    if (target == m_entries.end())
        return ShortcutResult::Err(error::Error(error::ErrorCode::InvalidArgument,
            "unknown shortcut id: " + id, /*showMsgBox=*/false));

    if (!sequence.isEmpty())
    {
        const auto conflict = std::find_if(m_entries.begin(), m_entries.end(),
            [&](const Entry& e) { return e.id != id && e.action->shortcut() == sequence; });
        if (conflict != m_entries.end())
            return ShortcutResult::Err(error::Error(error::ErrorCode::InvalidArgument,
                "already assigned to \"" + conflict->label.toStdString() + "\"",
                /*showMsgBox=*/false));
    }

    target->action->setShortcut(sequence);
    SaveOverrides();
    return ShortcutResult::Ok({});
}

void ShortcutManager::ResetToDefault(const std::string& id)
{
    const auto target = std::find_if(m_entries.begin(), m_entries.end(),
        [&](const Entry& e) { return e.id == id; });
    if (target == m_entries.end()) return;

    target->action->setShortcut(target->defaultSequence);
    SaveOverrides();
}

void ShortcutManager::ResetAllToDefaults()
{
    for (auto& e : m_entries)
        e.action->setShortcut(e.defaultSequence);
    SaveOverrides();
}

void ShortcutManager::Clear()
{
    m_entries.clear();
}

void ShortcutManager::LoadOverrides()
{
    try
    {
        const std::string path = DefaultFilePath();
        std::ifstream ifs(path);
        if (!ifs.is_open())
        {
            util::Logger::Debug("[ShortcutManager] No keybindings file found at '{}' (first run)", path);
            return;
        }

        nlohmann::json j;
        ifs >> j;
        if (!j.is_object())
        {
            util::Logger::Warn("[ShortcutManager] Keybindings file '{}' does not contain a JSON object", path);
            return;
        }

        for (const auto& [id, seqStr] : j.items())
        {
            if (!seqStr.is_string()) continue;
            m_pendingOverrides.emplace_back(id,
                QKeySequence::fromString(QString::fromStdString(seqStr.get<std::string>())));
        }
        util::Logger::Info("[ShortcutManager] Loaded {} keybinding override(s) from '{}'",
            m_pendingOverrides.size(), path);
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[ShortcutManager] Error loading keybindings file: {}", e.what());
    }
}

void ShortcutManager::SaveOverrides() const
{
    try
    {
        const std::string path = DefaultFilePath();
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());

        nlohmann::json j = nlohmann::json::object();
        for (const auto& e : m_entries)
        {
            const QKeySequence current = e.action->shortcut();
            if (current != e.defaultSequence)
                j[e.id] = current.toString().toStdString();
        }

        std::ofstream ofs(path);
        if (!ofs) throw std::runtime_error("cannot open for writing");
        ofs << j.dump(2);
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[ShortcutManager] Failed to save keybindings file: {}", e.what());
    }
}

std::string ShortcutManager::DefaultFilePath()
{
    return (config::GetConfig().GetDefaultAppPath() / "keybindings.json").string();
}

} // namespace ui::qt
