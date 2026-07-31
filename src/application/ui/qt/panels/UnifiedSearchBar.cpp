#include "UnifiedSearchBar.hpp"
#include "EventsContainer.hpp"
#include "events/EventsTableView.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCompleter>
#include <QStringListModel>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QIcon>

namespace ui::qt {

UnifiedSearchBar::UnifiedSearchBar(QWidget* parent)
    : QWidget(parent)
{
    // Initialize settings for search history
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_settings = std::make_unique<QSettings>(configPath + "/search.ini", QSettings::IniFormat);

    CreateLayout();
    LoadSearchHistory();

    // Connect internal signals
    connect(m_searchInput, &QLineEdit::textChanged, this, &UnifiedSearchBar::OnSearchTextChanged);
    connect(m_searchInput, &QLineEdit::returnPressed, this, &UnifiedSearchBar::OnSearchReturn);
    connect(m_clearButton, &QPushButton::clicked, this, &UnifiedSearchBar::OnClearClicked);
}

void UnifiedSearchBar::CreateLayout()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(8);

    // Search icon + input
    auto* searchIconLabel = new QLabel("🔍");
    mainLayout->addWidget(searchIconLabel);

    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText("Search events... (Ctrl+F)");
    m_searchInput->setMinimumWidth(300);

    // Create completer for search history
    auto* completerModel = new QStringListModel(m_searchHistory, this);
    m_completer = new QCompleter(completerModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_searchInput->setCompleter(m_completer);

    mainLayout->addWidget(m_searchInput);

    // Match counter
    m_matchCountLabel = new QLabel("0/0");
    m_matchCountLabel->setStyleSheet("color: #666; font-size: 11px; min-width: 50px;");
    mainLayout->addWidget(m_matchCountLabel);

    // Filter button
    m_filterButton = new QPushButton("🔽");
    m_filterButton->setToolTip("Apply as filter");
    m_filterButton->setMaximumWidth(32);
    connect(m_filterButton, &QPushButton::clicked, this, &UnifiedSearchBar::OnQuickFilterClicked);
    mainLayout->addWidget(m_filterButton);

    // Clear button
    m_clearButton = new QPushButton("✕");
    m_clearButton->setToolTip("Clear search");
    m_clearButton->setMaximumWidth(32);
    mainLayout->addWidget(m_clearButton);

    mainLayout->addStretch();
}

void UnifiedSearchBar::SetEventsSource(db::EventsContainer* events)
{
    m_events = events;
    UpdateMatchCount();
}

void UnifiedSearchBar::SetEventsView(EventsTableView* view)
{
    m_eventsView = view;
}

void UnifiedSearchBar::FocusSearchInput()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

QString UnifiedSearchBar::GetSearchQuery() const
{
    return m_searchInput->text();
}

void UnifiedSearchBar::ClearSearch()
{
    m_searchInput->clear();
    m_matchCountLabel->setText("0/0");
}

void UnifiedSearchBar::OnSearchTextChanged(const QString& text)
{
    m_lastQuery = text;
    UpdateMatchCount();
    emit SearchChanged(text);
}

void UnifiedSearchBar::OnSearchReturn()
{
    // When user presses Enter, apply search as filter
    if (!m_searchInput->text().isEmpty())
    {
        AddToSearchHistory(m_searchInput->text());
        emit FilterRequested("*", m_searchInput->text());
    }
}

void UnifiedSearchBar::OnClearClicked()
{
    ClearSearch();
}

void UnifiedSearchBar::OnSearchHistoryItemClicked(const QString& text)
{
    m_searchInput->setText(text);
    FocusSearchInput();
}

void UnifiedSearchBar::OnQuickFilterClicked()
{
    OnSearchReturn();
}

void UnifiedSearchBar::OnSearchSuggestionClicked(const QString& suggestion)
{
    m_searchInput->setText(suggestion);
    FocusSearchInput();
}

void UnifiedSearchBar::UpdateMatchCount()
{
    if (!m_events)
    {
        m_matchCountLabel->setText("0/0");
        return;
    }

    QString query = m_searchInput->text();
    int matchCount = CountMatches(query);
    int totalCount = static_cast<int>(m_events->Size());

    m_matchCountLabel->setText(QString("%1/%2").arg(matchCount).arg(totalCount));

    if (matchCount != m_lastMatchCount)
    {
        m_lastMatchCount = matchCount;
        emit MatchCountChanged(matchCount, totalCount);
    }
}

int UnifiedSearchBar::CountMatches(const QString& query)
{
    if (!m_events || query.isEmpty())
        return m_events ? static_cast<int>(m_events->Size()) : 0;

    int count = 0;
    QString lowerQuery = query.toLower();

    try
    {
        for (size_t i = 0; i < m_events->Size(); ++i)
        {
            const auto& event = m_events->GetEvent(i);
            bool matches = false;

            // Search across all event fields
            for (const auto& [key, value] : event.getEventItems())
            {
                if (QString::fromStdString(value).toLower().contains(lowerQuery))
                {
                    matches = true;
                    break;
                }
            }

            if (matches)
                count++;
        }
    }
    catch (const std::exception&)
    {
        // Gracefully handle errors
    }

    return count;
}

void UnifiedSearchBar::LoadSearchHistory()
{
    if (!m_settings)
        return;

    m_settings->beginGroup("SearchHistory");
    int size = m_settings->beginReadArray("items");
    for (int i = 0; i < size; ++i)
    {
        m_settings->setArrayIndex(i);
        QString query = m_settings->value("query", "").toString();
        if (!query.isEmpty())
            m_searchHistory.append(query);
    }
    m_settings->endArray();
    m_settings->endGroup();

    // Update completer
    if (auto model = qobject_cast<QStringListModel*>(m_completer->model()))
    {
        model->setStringList(m_searchHistory);
    }
}

void UnifiedSearchBar::SaveSearchHistory()
{
    if (!m_settings)
        return;

    m_settings->beginGroup("SearchHistory");
    m_settings->beginWriteArray("items");
    for (int i = 0; i < m_searchHistory.size(); ++i)
    {
        m_settings->setArrayIndex(i);
        m_settings->setValue("query", m_searchHistory[i]);
    }
    m_settings->endArray();
    m_settings->endGroup();
    m_settings->sync();
}

void UnifiedSearchBar::AddToSearchHistory(const QString& query)
{
    if (query.isEmpty() || m_searchHistory.contains(query))
        return;

    // Add to front
    m_searchHistory.insert(0, query);

    // Keep only last N items
    while (m_searchHistory.size() > MAX_HISTORY)
        m_searchHistory.removeLast();

    SaveSearchHistory();

    // Update completer
    if (auto model = qobject_cast<QStringListModel*>(m_completer->model()))
    {
        model->setStringList(m_searchHistory);
    }
}

void UnifiedSearchBar::CreateFilterPresets()
{
    // Placeholder for quick filter presets
    // Could be: Errors, Warnings, My Service, Recent errors, etc.
}

} // namespace ui::qt
