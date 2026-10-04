#pragma once

#include <QTabWidget>
#include <QWidget>

#include <map>
#include <string>

namespace ui::qt::utils
{

/// Removes every page listed in @p tracked (id -> page) from @p tabs, schedules
/// it for deletion and clears @p tracked.
///
/// Pages are looked up by widget, so tabs that are not tracked (the built-in
/// Events, Statistics, ... tabs) stay untouched wherever they sit, even after
/// the tabs were reordered.
inline void RemoveTrackedTabs(QTabWidget* tabs, std::map<std::string, QWidget*>& tracked)
{
    for (const auto& [id, page] : tracked)
    {
        if (!page)
            continue;
        if (tabs)
        {
            const int index = tabs->indexOf(page);
            if (index >= 0)
                tabs->removeTab(index);
        }
        page->deleteLater();
    }
    tracked.clear();
}

} // namespace ui::qt::utils
