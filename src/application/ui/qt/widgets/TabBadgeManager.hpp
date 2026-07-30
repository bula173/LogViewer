#pragma once

#include <QTabWidget>
#include <QString>
#include <map>
#include <memory>

namespace ui::qt
{

/**
 * @brief Manages activity badges on QTabWidget tabs.
 *
 * Shows a red dot (●) on tabs when they contain updated data.
 * Auto-clears when user switches to that tab.
 *
 * Usage:
 *   auto badgeManager = std::make_unique<TabBadgeManager>(tabWidget);
 *   badgeManager->SetBadge(0, "new");  // Show badge on tab 0
 *   badgeManager->ClearBadge(0);       // Hide badge on tab 0
 */
class TabBadgeManager : public QObject
{
    Q_OBJECT

  public:
    explicit TabBadgeManager(QTabWidget* tabWidget);
    ~TabBadgeManager() = default;

    /**
     * @brief Show a badge on a tab indicating new/updated data.
     * @param tabIndex Index of the tab to badge
     */
    void SetBadge(int tabIndex);

    /**
     * @brief Hide the badge on a tab.
     * @param tabIndex Index of the tab
     */
    void ClearBadge(int tabIndex);

    /**
     * @brief Check if a tab has an active badge.
     * @param tabIndex Index of the tab
     * @return true if badge is visible
     */
    bool HasBadge(int tabIndex) const;

  private Q_SLOTS:
    void OnTabChanged(int index);

  private:
    void UpdateTabText(int index);

    QTabWidget* m_tabWidget;
    std::map<int, QString> m_originalTabText;  // Store original tab names
    std::map<int, bool> m_badges;              // Track which tabs have badges
};

} // namespace ui::qt
