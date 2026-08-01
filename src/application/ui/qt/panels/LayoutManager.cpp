#include "LayoutManager.hpp"

#include "Logger.hpp"

#include <QSettings>
#include <algorithm>

namespace ui::qt {

// ---------------------------------------------------------------------------
// Predefined layouts
// ---------------------------------------------------------------------------

static LayoutDescriptor MakeXmlLogLayout()
{
    LayoutDescriptor d;
    d.name      = "XML Log Analysis";
    d.isBuiltIn = true;
    d.tabVisibility = {
        {"Events",     true},
        {"Statistics", true},
        {"Patterns",   true},
        {"Timeline",   true},
        {"Actors",     true},
        {"Signals",    false},
        {"Traces",     false},
        {"Bookmarks",  false},
        {"Scenarios",  false},
    };
    d.activeTab          = "Events";
    d.filtersDockVisible = true;
    d.detailsDockVisible = true;
    d.bottomDockVisible  = false;
    return d;
}

static LayoutDescriptor MakeCanBusLayout()
{
    LayoutDescriptor d;
    d.name      = "CAN Bus Analysis";
    d.isBuiltIn = true;
    d.tabVisibility = {
        {"Events",     true},
        {"Statistics", true},
        {"Signals",    true},
        {"Timeline",   true},
        {"Traces",     true},
        {"Patterns",   false},
        {"Actors",     false},
        {"Bookmarks",  false},
        {"Scenarios",  false},
    };
    d.activeTab          = "Signals";
    d.filtersDockVisible = true;
    d.detailsDockVisible = true;
    d.bottomDockVisible  = false;
    return d;
}

// static
std::vector<LayoutDescriptor> LayoutManager::BuiltInLayouts()
{
    return {MakeXmlLogLayout(), MakeCanBusLayout()};
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

LayoutManager::LayoutManager()
{
    LoadFromSettings();
}

// ---------------------------------------------------------------------------
// User layout CRUD
// ---------------------------------------------------------------------------

const std::vector<LayoutDescriptor>& LayoutManager::UserLayouts() const
{
    return m_userLayouts;
}

void LayoutManager::Save(LayoutDescriptor layout)
{
    const std::string name = layout.name.toStdString();
    util::Logger::Debug("[LayoutManager] Save layout: '{}'", name);

    auto it = std::ranges::find_if(m_userLayouts,
        [&](const LayoutDescriptor& d) { return d.name == layout.name; });
    if (it != m_userLayouts.end())
    {
        util::Logger::Debug("[LayoutManager] Overwriting existing layout '{}'", name);
        *it = std::move(layout);
    }
    else
    {
        m_userLayouts.push_back(std::move(layout));
    }
    SaveToSettings();
    util::Logger::Info("[LayoutManager] Layout '{}' saved ({} user layouts total)",
                       name, m_userLayouts.size());
}

void LayoutManager::Remove(const QString& name)
{
    util::Logger::Debug("[LayoutManager] Remove layout: '{}'", name.toStdString());

    auto it = std::ranges::find_if(m_userLayouts,
        [&](const LayoutDescriptor& d) { return d.name == name; });
    if (it == m_userLayouts.end())
    {
        util::Logger::Warn("[LayoutManager] Layout '{}' not found — nothing to remove",
                           name.toStdString());
        return;
    }
    m_userLayouts.erase(it);
    SaveToSettings();
    util::Logger::Info("[LayoutManager] Layout '{}' removed ({} user layouts remaining)",
                       name.toStdString(), m_userLayouts.size());
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

void LayoutManager::SaveToSettings() const
{
    util::Logger::Debug("[LayoutManager] SaveToSettings: persisting {} layout(s)",
                        m_userLayouts.size());

    QSettings s("LogViewer", "LogViewer");
    s.beginGroup("userLayouts");
    s.remove("");   // wipe stale entries

    QStringList names;
    for (const auto& d : m_userLayouts) {
        names << d.name;
        s.beginGroup(d.name);
        s.setValue("windowState",       d.windowState);
        s.setValue("activeTab",         d.activeTab);
        s.setValue("filtersDockVisible", d.filtersDockVisible);
        s.setValue("detailsDockVisible", d.detailsDockVisible);
        s.setValue("bottomDockVisible",  d.bottomDockVisible);
        s.beginGroup("tabs");
        for (auto it = d.tabVisibility.constBegin(); it != d.tabVisibility.constEnd(); ++it)
            s.setValue(it.key(), it.value());
        s.endGroup(); // tabs
        s.endGroup(); // <name>
    }
    s.setValue("names", names);
    s.endGroup(); // userLayouts
}

void LayoutManager::LoadFromSettings()
{
    util::Logger::Debug("[LayoutManager] LoadFromSettings: reading user layouts");

    m_userLayouts.clear();
    QSettings s("LogViewer", "LogViewer");
    s.beginGroup("userLayouts");
    const QStringList names = s.value("names").toStringList();
    for (const QString& name : names) {
        if (!s.childGroups().contains(name))
        {
            util::Logger::Warn("[LayoutManager] Layout '{}' listed in names but has no settings group — skipping",
                               name.toStdString());
            continue;
        }
        s.beginGroup(name);
        LayoutDescriptor d;
        d.name               = name;
        d.isBuiltIn          = false;
        d.windowState        = s.value("windowState").toByteArray();
        d.activeTab          = s.value("activeTab").toString();
        d.filtersDockVisible = s.value("filtersDockVisible", true).toBool();
        d.detailsDockVisible = s.value("detailsDockVisible", true).toBool();
        d.bottomDockVisible  = s.value("bottomDockVisible",  false).toBool();
        s.beginGroup("tabs");
        for (const QString& k : s.childKeys())
            d.tabVisibility[k] = s.value(k).toBool();
        s.endGroup(); // tabs
        s.endGroup(); // <name>
        m_userLayouts.push_back(std::move(d));
    }
    s.endGroup(); // userLayouts

    util::Logger::Info("[LayoutManager] Loaded {} user layout(s) from settings",
                       m_userLayouts.size());
}

} // namespace ui::qt
