#pragma once

#include <QComboBox>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QWidget>
#include <vector>

namespace db {
class EventsContainer;
}

namespace ui::qt {
class EventsTableView;
}

namespace ui::qt {

/**
 * @brief Panel for detecting recurring structures and correlations across log events.
 *
 * Three analysis modes, all scoped to the currently visible (filtered) event set:
 *
 * @par 1. Message Templates
 * Extracts the common skeleton of log messages: constant tokens are kept as-is,
 * variable tokens are replaced with @c {*}.  Clicking a template row highlights
 * matching events in the events table.
 *
 * Algorithm (simplified Drain-inspired):
 *  - Events are grouped by their type-field value.
 *  - Within each group the message field is tokenised on whitespace / punctuation.
 *  - Tokens that differ across instances of the same template are replaced with @c {*}.
 *
 * @par 2. Co-occurrence (Correlation)
 * Shows pairs of event-type values that frequently appear within the same
 * configurable time window.  Result is a table sorted by co-occurrence count
 * descending.  Useful for spotting causal chains: e.g. "ERROR X is almost
 * always preceded within 2 s by WARNING Y".
 *
 * @par 3. Sequence N-grams
 * Extracts the sequence of event-type values in chronological order and counts
 * bigrams and trigrams.  The most frequent patterns reveal standard operating
 * procedures, boot sequences, or anomalous repetitions.
 *
 * Refreshes automatically via MainWindow::modelReset signal.
 */
class PatternAnalysisPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit PatternAnalysisPanel(db::EventsContainer& events,
                                  EventsTableView*     eventsView,
                                  QWidget*             parent = nullptr);

  public slots:
    /// Recompute all analyses from the current visible event set.
    void Refresh();

  signals:
    /// Emitted when the user clicks a template row; contains the indices of
    /// matching events so MainWindow can forward them to SetFilteredEvents().
    void TemplateSelected(std::vector<unsigned long> matchingIndices);

  private:
    // ── Layout ───────────────────────────────────────────────────────────
    void BuildLayout();

    // ── Per-mode refresh ─────────────────────────────────────────────────
    void RefreshTemplates  (const std::vector<unsigned long>& indices);
    void RefreshCooccurrence(const std::vector<unsigned long>& indices);
    void RefreshNgrams     (const std::vector<unsigned long>& indices);

    // ── Helpers ───────────────────────────────────────────────────────────
    [[nodiscard]] std::vector<unsigned long> VisibleIndices() const;

    /// Tokenise @p text on whitespace and common delimiters.
    [[nodiscard]] static std::vector<std::string> Tokenise(const std::string& text);

    /// Merge two token sequences into a template; differing tokens → "{*}".
    [[nodiscard]] static std::vector<std::string> MergeTokens(
        const std::vector<std::string>& a,
        const std::vector<std::string>& b);

    /// Render a token sequence back to a human-readable string.
    [[nodiscard]] static QString RenderTemplate(const std::vector<std::string>& tokens);

    // ── Widgets ───────────────────────────────────────────────────────────
    db::EventsContainer& m_events;
    EventsTableView*      m_eventsView;

    QTabWidget*   m_tabs           {nullptr};

    // Templates tab
    QComboBox*    m_msgFieldCombo  {nullptr}; ///< Which field to use as "message"
    QTableWidget* m_templateTable  {nullptr};

    // Co-occurrence tab
    QSpinBox*     m_windowSpin     {nullptr}; ///< Time-window size in seconds
    QTableWidget* m_coocTable      {nullptr};

    // N-gram tab
    QComboBox*    m_ngramSizeCombo {nullptr}; ///< Bigram / Trigram selector
    QSpinBox*     m_ngramTopNSpin  {nullptr}; ///< How many top n-grams to display
    QTableWidget* m_ngramTable     {nullptr};

    // Per-template index map: table-row → matching event indices
    std::vector<std::vector<unsigned long>> m_templateMatches;
};

} // namespace ui::qt
