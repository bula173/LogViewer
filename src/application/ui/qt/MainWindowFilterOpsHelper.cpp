#include "MainWindowFilterOpsHelper.hpp"
#include "MainWindow.hpp"
#include "FilterManager.hpp"
#include "EventsContainer.hpp"
#include "Logger.hpp"

namespace ui::qt {

MainWindowFilterOpsHelper::MainWindowFilterOpsHelper(MainWindow* mainWindow)
    : m_mainWindow(mainWindow),
      m_filterManager(&filters::FilterManager::getInstance()),
      m_eventsContainer(nullptr)
{
    if (!m_mainWindow) {
        util::Logger::Error("[FilterOpsHelper] MainWindow pointer is null");
    }
    if (!m_filterManager) {
        util::Logger::Error("[FilterOpsHelper] FilterManager not available");
    }
}

void MainWindowFilterOpsHelper::OnApplyFilterClicked()
{
    if (!m_mainWindow || !m_filterManager) return;

    util::Logger::Debug("[FilterOpsHelper] Apply filter clicked");
    ApplyFilterInternal();
    RefreshFilterIndicators();
}

void MainWindowFilterOpsHelper::OnExtendedFiltersChanged()
{
    if (!m_mainWindow || !m_filterManager) return;

    util::Logger::Debug("[FilterOpsHelper] Extended filters changed");
    ApplyFilterInternal();
    RefreshFilterIndicators();
}

void MainWindowFilterOpsHelper::OnSearchRequested()
{
    if (!m_mainWindow || !m_filterManager) return;

    util::Logger::Debug("[FilterOpsHelper] Search requested");
    ApplyFilterInternal();
    RefreshFilterIndicators();
}

void MainWindowFilterOpsHelper::OnSearchResultActivated(long eventId)
{
    if (!m_mainWindow || !m_eventsContainer) return;

    util::Logger::Debug("[FilterOpsHelper] Search result activated for event {}", eventId);
    // Delegate to MainWindow to navigate to event
}

void MainWindowFilterOpsHelper::ApplyExtendedFilters()
{
    if (!m_mainWindow || !m_filterManager) return;

    util::Logger::Debug("[FilterOpsHelper] Applying extended filters");
    ApplyFilterInternal();
}

void MainWindowFilterOpsHelper::UpdateFilterStatus(int totalEvents, int filteredCount,
                                                   const QString& filterName)
{
    m_lastFilteredCount = filteredCount;
    m_lastFilterName = filterName;

    util::Logger::Debug("[FilterOpsHelper] Filter status: {}/{} events, filter: {}",
        filteredCount, totalEvents, filterName.toStdString());

    RefreshFilterIndicators();
}

bool MainWindowFilterOpsHelper::HasActiveFilters() const
{
    if (!m_filterManager) return false;

    const auto& filters = m_filterManager->getFilters();
    return !filters.empty();
}

void MainWindowFilterOpsHelper::ApplyFilterInternal()
{
    if (!m_mainWindow || !m_filterManager || !m_eventsContainer) return;

    util::Logger::Trace("[FilterOpsHelper] Applying filters internally");

    // Delegate to MainWindow for actual filter application
    // This method coordinates the filter operation
}

void MainWindowFilterOpsHelper::RefreshFilterIndicators()
{
    if (!m_mainWindow) return;

    util::Logger::Trace("[FilterOpsHelper] Refreshing filter indicators");

    // Update UI indicators showing active filters
    // Delegate to MainWindow to update status bar/panel
}

} // namespace ui::qt
