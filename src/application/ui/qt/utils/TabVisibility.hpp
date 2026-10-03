#pragma once

#include <QAction>
#include <QPointer>
#include <QTabWidget>

namespace ui::qt::utils
{

/// Makes @p action show/hide the tab that holds @p page.
///
/// The tab is looked up by its widget each time the action fires. A tab
/// position captured when the menu is built goes stale as soon as tabs are
/// sorted or removed, and then hides a different tab.
inline void BindTabVisibilityAction(QAction* action, QTabWidget* tabs, QWidget* page)
{
    QObject::connect(action, &QAction::toggled, action, [tabs = QPointer<QTabWidget>(tabs),
                                                          page = QPointer<QWidget>(page)](bool visible) {
        if (!tabs || !page)
            return;
        const int index = tabs->indexOf(page);
        if (index >= 0)
            tabs->tabBar()->setTabVisible(index, visible);
    });
}

} // namespace ui::qt::utils
