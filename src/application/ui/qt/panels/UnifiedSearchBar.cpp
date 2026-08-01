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
#include <QDir>
#include <QDebug>
#include <QTimer>

namespace ui::qt {

UnifiedSearchBar::UnifiedSearchBar(QWidget* parent)
    : QWidget(parent)
{
    // Initialize settings for search history
    try
    {
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (configPath.isEmpty())
        {
            // Fallback to home directory if AppDataLocation is not available
            configPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        }

        if (!configPath.isEmpty())
        {
            // Ensure directory exists
            QDir(configPath).mkpath(".");
            m_settings = std::make_unique<QSettings>(configPath + "/search.ini", QSettings::IniFormat);
        }
    }
    catch (const std::exception& e)
    {
        qWarning() << "Failed to initialize search settings:" << e.what();
    }

    CreateLayout();
    LoadSearchHistory();

    // Verify all UI components were created successfully
    Q_ASSERT_X(m_searchInput != nullptr, "UnifiedSearchBar", "m_searchInput failed to create");
    Q_ASSERT_X(m_clearButton != nullptr, "UnifiedSearchBar", "m_clearButton failed to create");
    Q_ASSERT_X(m_completer != nullptr, "UnifiedSearchBar", "m_completer failed to create");

    // Initialize match count debounce timer to avoid O(n*m) on every keystroke
    m_matchCountDebounceTimer = new QTimer(this);
    Q_ASSERT_X(m_matchCountDebounceTimer != nullptr, "UnifiedSearchBar", "Failed to create debounce timer");
    m_matchCountDebounceTimer->setSingleShot(true);
    m_matchCountDebounceTimer->setInterval(MATCH_COUNT_DEBOUNCE_MS);
    connect(m_matchCountDebounceTimer, &QTimer::timeout, this, &UnifiedSearchBar::UpdateMatchCount);

    // Connect internal signals
    Q_ASSERT(m_searchInput != nullptr);
    connect(m_searchInput, &QLineEdit::textChanged, this, &UnifiedSearchBar::OnSearchTextChanged);
    connect(m_searchInput, &QLineEdit::returnPressed, this, &UnifiedSearchBar::OnSearchReturn);

    Q_ASSERT(m_clearButton != nullptr);
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

    // Details button
    auto* detailsButton = new QPushButton("⚙ Details...");
    detailsButton->setToolTip("Advanced search options (regex, case-sensitive, columns)");
    detailsButton->setMaximumWidth(100);
    connect(detailsButton, &QPushButton::clicked, this, &UnifiedSearchBar::OnDetailsClicked);
    mainLayout->addWidget(detailsButton);

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
    if (m_searchInput)
    {
        m_searchInput->setFocus();
        m_searchInput->selectAll();
    }
}

QString UnifiedSearchBar::GetSearchQuery() const
{
    if (!m_searchInput)
        return QString();
    return m_searchInput->text();
}

void UnifiedSearchBar::ClearSearch()
{
    if (m_searchInput)
        m_searchInput->clear();
    if (m_matchCountLabel)
        m_matchCountLabel->setText("0/0");
}

void UnifiedSearchBar::OnSearchTextChanged(const QString& text)
{
    m_lastQuery = text;

    // Debounce match count update to avoid O(n*m) computation on every keystroke
    if (m_matchCountDebounceTimer) {
        m_matchCountDebounceTimer->start();
    } else {
        UpdateMatchCount();
    }

    emit SearchChanged(text);
}

void UnifiedSearchBar::OnSearchReturn()
{
    // When user presses Enter, apply search as filter
    if (m_searchInput && !m_searchInput->text().isEmpty())
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
    if (m_searchInput)
        m_searchInput->setText(text);
    FocusSearchInput();
}

void UnifiedSearchBar::OnQuickFilterClicked()
{
    OnSearchReturn();
}

void UnifiedSearchBar::OnSearchSuggestionClicked(const QString& suggestion)
{
    if (m_searchInput)
        m_searchInput->setText(suggestion);
    FocusSearchInput();
}

void UnifiedSearchBar::UpdateMatchCount()
{
    if (!m_matchCountLabel)
        return;

    if (!m_events)
    {
        m_matchCountLabel->setText("0/0");
        return;
    }

    if (!m_searchInput)
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
        const size_t totalEvents = m_events->Size();
        for (size_t i = 0; i < totalEvents; ++i)
        {
            try
            {
                // Use thread-safe access to EventsContainer
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
            catch (const std::exception&)
            {
                // Skip events that fail to access (removed or invalid)
                continue;
            }
        }
    }
    catch (const std::exception&)
    {
        // Gracefully handle container-level errors (resizing, etc.)
    }

    return count;
}

void UnifiedSearchBar::LoadSearchHistory()
{
    if (!m_settings)
        return;

    try
    {
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
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error loading search history:" << e.what();
    }

    // Update completer
    if (m_completer)
    {
        if (auto model = qobject_cast<QStringListModel*>(m_completer->model()))
        {
            model->setStringList(m_searchHistory);
        }
    }
}

void UnifiedSearchBar::SaveSearchHistory()
{
    if (!m_settings)
        return;

    try
    {
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

        // Check if sync was successful
        if (m_settings->status() != QSettings::NoError) {
            qWarning() << "Failed to save search history, QSettings status:" << m_settings->status();
        }
    }
    catch (const std::exception& e)
    {
        qWarning() << "Error saving search history:" << e.what();
    }
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
    if (m_completer)
    {
        if (auto model = qobject_cast<QStringListModel*>(m_completer->model()))
        {
            model->setStringList(m_searchHistory);
        }
    }
}

void UnifiedSearchBar::CreateFilterPresets()
{
    // Placeholder for quick filter presets
    // Could be: Errors, Warnings, My Service, Recent errors, etc.
}

void UnifiedSearchBar::OnDetailsClicked()
{
    // TODO: Open SearchDetailsDialog with advanced options:
    // - Regex checkbox
    // - Case-sensitive checkbox
    // - Column selector
    // - Search mode (substring, regex, advanced)
    // - Save/load search presets

    qDebug() << "Details button clicked - advanced search dialog (TODO)";
    // For now, just log that the button was clicked
    // This will be expanded to open a proper dialog
}

} // namespace ui::qt
