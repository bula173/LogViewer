#include "TabBadgeManager.hpp"

#include <QTabBar>

namespace ui::qt
{

TabBadgeManager::TabBadgeManager(QTabWidget* tabWidget)
    : m_tabWidget(tabWidget)
{
    if (!m_tabWidget)
        return;

    // Store original tab text for all tabs
    for (int i = 0; i < m_tabWidget->count(); ++i)
    {
        m_originalTabText[i] = m_tabWidget->tabText(i);
        m_badges[i] = false;
    }

    // Clear badge when user switches to a tab
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &TabBadgeManager::OnTabChanged);
}

void TabBadgeManager::SetBadge(int tabIndex)
{
    if (!m_tabWidget || tabIndex < 0 || tabIndex >= m_tabWidget->count())
        return;

    if (m_badges[tabIndex])
        return;  // Already has badge

    m_badges[tabIndex] = true;
    UpdateTabText(tabIndex);
}

void TabBadgeManager::ClearBadge(int tabIndex)
{
    if (!m_tabWidget || tabIndex < 0 || tabIndex >= m_tabWidget->count())
        return;

    if (!m_badges[tabIndex])
        return;  // Doesn't have badge

    m_badges[tabIndex] = false;
    UpdateTabText(tabIndex);
}

bool TabBadgeManager::HasBadge(int tabIndex) const
{
    auto it = m_badges.find(tabIndex);
    return it != m_badges.end() && it->second;
}

void TabBadgeManager::OnTabChanged(int index)
{
    // Clear badge when user switches to this tab
    if (index >= 0 && HasBadge(index))
    {
        ClearBadge(index);
    }
}

void TabBadgeManager::UpdateTabText(int index)
{
    if (!m_tabWidget || index < 0 || index >= m_tabWidget->count())
        return;

    const QString originalText = m_originalTabText[index];
    const bool hasBadge = m_badges[index];

    if (hasBadge)
    {
        // Add red dot badge to tab text
        m_tabWidget->setTabText(index, originalText + " ●");
    }
    else
    {
        // Restore original tab text
        m_tabWidget->setTabText(index, originalText);
    }
}

} // namespace ui::qt
