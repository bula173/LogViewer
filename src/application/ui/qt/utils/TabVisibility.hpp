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
    action->setData(QVariant::fromValue<QObject*>(page));
    QObject::connect(action, &QAction::toggled, action, [tabs = QPointer<QTabWidget>(tabs),
                                                          page = QPointer<QWidget>(page)](bool visible) {
        if (!tabs || !page)
            return;
        const int index = tabs->indexOf(page);
        if (index >= 0)
            tabs->tabBar()->setTabVisible(index, visible);
    });
}

/// Ticks each action bound by BindTabVisibilityAction() according to whether
/// its own tab is visible. Matched by page, not by tab text: a plugin tab may
/// carry the same label as a built-in one.
inline void SyncTabVisibilityActions(const QList<QAction*>& actions, QTabWidget* tabs)
{
    if (!tabs)
        return;
    for (QAction* action : actions)
    {
        auto* page = qobject_cast<QWidget*>(action->data().value<QObject*>());
        const int index = page ? tabs->indexOf(page) : -1;
        if (index >= 0)
            action->setChecked(tabs->tabBar()->isTabVisible(index));
    }
}

} // namespace ui::qt::utils
