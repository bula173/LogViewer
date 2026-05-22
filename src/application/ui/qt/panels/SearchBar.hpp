#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;
class QToolButton;

namespace ui::qt {

/**
 * @brief Compact browser-style inline search bar for the events table.
 *
 * Hidden by default. Show with FocusInput() (typically triggered by Ctrl+F).
 * Emits SearchChanged whenever the text or case-sensitivity flag changes.
 * The owner is responsible for calling SetMatchInfo() to update the counter.
 */
class SearchBar : public QWidget
{
    Q_OBJECT

  public:
    explicit SearchBar(QWidget* parent = nullptr);

    /// Update the "current / total" match counter label.
    void SetMatchInfo(int current, int total);

    /// Show the bar and focus the input field.
    void FocusInput();

    /// Clear the text and hide the bar.
    void CloseBar();

  signals:
    void SearchChanged(const QString& term, bool caseSensitive);
    void NavigatePrev();
    void NavigateNext();
    void Closed();

  private slots:
    void HandleTextChanged(const QString& text);
    void HandleCaseToggled();

  private:
    QLineEdit*   m_edit       {nullptr};
    QLabel*      m_matchLabel {nullptr};
    QPushButton* m_prevBtn    {nullptr};
    QPushButton* m_nextBtn    {nullptr};
    QToolButton* m_caseBtn    {nullptr};
    QPushButton* m_closeBtn   {nullptr};
};

} // namespace ui::qt
