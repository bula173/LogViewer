#pragma once

#include "ui/IMainWindowView.hpp"
#include "ui/IUiPanels.hpp"
#include "config/ConfigObserver.hpp"
#include "ai/AIServiceFactory.hpp"

#include <QMainWindow>
#include <memory>

class QLineEdit;
class QPushButton;
class QProgressBar;
class QLabel;
class QSplitter;
class QTabWidget;
class QWidget;
class QDragEnterEvent;
class QDropEvent;
class QDockWidget;

namespace mvc
{
class IController;
}

namespace db
{
class EventsContainer;
}

namespace ai
{
class IAIService;
class LogAnalyzer;
}

namespace ui
{
class MainWindowPresenter;
}

namespace ui::qt
{

class SearchResultsView;
class TypeFilterView;
class ItemDetailsView;
class EventsTableView;
class FiltersPanel;
class AIAnalysisPanel;
class AIConfigPanel;

class MainWindow : public QMainWindow,
                   public ui::IMainWindowView,
                   public ui::ISearchResultsViewObserver,
                   public config::ConfigObserver
{
    Q_OBJECT

  public:
    MainWindow(mvc::IController& controller, db::EventsContainer& events,
        QWidget* parent = nullptr);
    ~MainWindow() override;

    // IMainWindowView implementation
    std::string ReadSearchQuery() const override;
    std::string CurrentStatusText() const override;
    void UpdateStatusText(const std::string& text) override;
    void SetSearchControlsEnabled(bool enabled) override;
    void ToggleProgressVisibility(bool visible) override;
    void ConfigureProgressRange(int range) override;
    void UpdateProgressValue(int value) override;
    void ProcessPendingEvents() override;
    void RefreshLayout() override;

    // ISearchResultsViewObserver
    void OnSearchResultActivated(long eventId) override;

    // IConfigObserver
    void OnConfigChanged() override;

  private slots:
    void OnSearchRequested();
    void OnApplyFilterClicked();
    void HandleTypeFilterChanged();
    void OnExtendedFiltersChanged();
    void OnOpenFileRequested();
    void OnClearDataRequested();
    void OnOpenAppLogRequested();
    void OnExitRequested();
    void OnAboutRequested();

  private:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    void HandleDroppedFile(const QString& path);
    void InitializeUi(db::EventsContainer& events);
    void InitializePresenter(mvc::IController& controller,
        db::EventsContainer& events);
    void ApplyExtendedFilters();
    void SetupMenus();
    void ShowError(const QString& title, const QString& message);

    QLineEdit* m_searchEdit {nullptr};
    QPushButton* m_searchButton {nullptr};
    QProgressBar* m_progressBar {nullptr};
    QLabel* m_statusLabel {nullptr};
    SearchResultsView* m_searchResults {nullptr};
    QPushButton* m_applyFilterButton {nullptr};
    QSplitter* m_bottomSplitter {nullptr};
    QSplitter* m_leftSplitter {nullptr};
    QSplitter* m_rightSplitter {nullptr};
    QTabWidget* m_filterTabs {nullptr};
    EventsTableView* m_eventsView {nullptr};
    FiltersPanel* m_filtersPanel {nullptr};

    // Dock widgets for collapsible panels
    QDockWidget* m_filtersDock {nullptr};
    QDockWidget* m_detailsDock {nullptr};
    QDockWidget* m_bottomDock {nullptr};
    QDockWidget* m_aiConfigDock {nullptr};

    std::unique_ptr<ui::MainWindowPresenter> m_presenter;
    TypeFilterView* m_typeFilterView {nullptr};
    ItemDetailsView* m_itemDetailsView {nullptr};
    AIAnalysisPanel* m_aiPanel {nullptr};
    AIConfigPanel* m_aiConfigPanel {nullptr};
    db::EventsContainer* m_events {nullptr};
    
    // AI service shared between panels
    std::shared_ptr<ai::AIServiceWrapper> m_aiService;
    std::shared_ptr<ai::LogAnalyzer> m_aiAnalyzer;
};

} // namespace ui::qt
